#include <iostream>
#include <cstring>
#include <cstdio>
#include "../include/indices.h"

using namespace std;

// Arquivos binários que guardam os índices secundários e as listas invertidas.
// A separação em dois arquivos (Indice e Lista) é necessária porque o tamanho 
// do arquivo de Índice deve ser previsível para buscas, enquanto a Lista cresce dinamicamente.
static const char* ARQ_INDICE_DIRETOR = "dados/IndiceDiretor.dat";
static const char* ARQ_LISTA_DIRETOR  = "dados/ListaDiretor.dat";
static const char* ARQ_INDICE_GENERO  = "dados/IndiceGenero.dat";
static const char* ARQ_LISTA_GENERO   = "dados/ListaGenero.dat";
static const char* ARQ_DADOS_FILMES   = "dados/Filmes.dat";

// Reaproveitamento de nós (POP na pilha da LED)
// Antes de gravar um nó novo no final do arquivo, checamos o cabeçalho.
// Se 'topo_led' for diferente de -1, existe um "buraco" de uma exclusão passada.
// Pegamos esse offset, gravamos o novo nó por cima e atualizamos a LED para o próximo buraco.
static long alocar_no_lista(FILE* f_lista, NoLista novo_no) {
    CabecalhoLista cab;
    rewind(f_lista);                                   // volta pro início (cabeçalho)
    fread(&cab, sizeof(CabecalhoLista), 1, f_lista);    // lê cabeçalho

    long offset_no;
    
    // Verifica se existe espaço ocioso no disco para ser reaproveitado
    if (cab.topo_led != -1) {
        // Existe espaço ocioso. Pega o offset do topo da LED.
        offset_no = cab.topo_led;
        
        NoLista no_livre;
        fseek(f_lista, offset_no, SEEK_SET);            // vai até o buraco
        fread(&no_livre, sizeof(NoLista), 1, f_lista);   // lê o nó livre
        
        // O "próximo" do nó livre guardava o link para o resto da LED.
        // Atualizamos o cabeçalho para apontar para o próximo elemento livre.
        cab.topo_led = no_livre.proximo; 
    } else {
        // Arquivo está totalmente compactado. Grava fisicamente no EOF (Fim de arquivo).
        fseek(f_lista, 0, SEEK_END);
        offset_no = ftell(f_lista); // pega posição = fim do arquivo
    }

    // Escreve o nó no disco físico na posição calculada
    fseek(f_lista, offset_no, SEEK_SET);
    fwrite(&novo_no, sizeof(NoLista), 1, f_lista);

    // Salva o cabeçalho atualizado
    rewind(f_lista);
    fwrite(&cab, sizeof(CabecalhoLista), 1, f_lista);

    return offset_no; // Retorna onde o nó foi gravado para quem chamou a função
}

// Exclusão Lógica e Inserção na LED (PUSH na pilha)
// Quando um filme muda de diretor/gênero, o nó não é apagado do disco fisicamente (evita fragmentação externa).
// Ele é transformado em um nó "fantasma" que aponta para o antigo topo da LED.
static void liberar_no_lista(FILE* f_lista, long offset_no) {
    CabecalhoLista cab;
    rewind(f_lista);//inicio do arquivo
    fread(&cab, sizeof(CabecalhoLista), 1, f_lista); // lê cabeçalho

    // -1 indica visualmente/logicamente que é um registro deletado.
    NoLista no_livre;
    no_livre.id_filme = -1;           
    no_livre.proximo  = cab.topo_led; // O nó excluído aponta pro antigo topo da lista de buracos

    // Sobrescreve o nó antigo, transformando-o no fantasma
    fseek(f_lista, offset_no, SEEK_SET);
    fwrite(&no_livre, sizeof(NoLista), 1, f_lista);

    // O cabeçalho passa a apontar para este nó recém-liberado (novo topo da LED)
    cab.topo_led = offset_no;
    rewind(f_lista);
    fwrite(&cab, sizeof(CabecalhoLista), 1, f_lista);
}

// Inicializadores básicos
// Apenas garantem a criação dos arquivos caso o programa rode pela primeira vez.
// Tenta ler ("rb"), se falhar, cria um novo ("wb").
static void inicializar_arquivo_indice(const char* caminho) {
    FILE* f = fopen(caminho, "rb"); // tenta abrir p/ leitura
    if (!f) {
        f = fopen(caminho, "wb"); // não existe, cria vazio
        if (f) fclose(f);
    } else {
        fclose(f); // já existe
    }
}

// Inicializa o arquivo de lista encadeada já criando um cabeçalho zerado (topo_led = -1).
static void inicializar_arquivo_lista(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) {
        f = fopen(caminho, "wb+"); // cria arquivo novo
        if (f) {
            CabecalhoLista cab;
            cab.topo_led = -1; // LED vazia
            fwrite(&cab, sizeof(CabecalhoLista), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}

// Dispara a checagem/criação para todos os 4 arquivos secundários.
void inicializar_indices_secundarios() {
    inicializar_arquivo_indice(ARQ_INDICE_DIRETOR);
    inicializar_arquivo_lista(ARQ_LISTA_DIRETOR);
    inicializar_arquivo_indice(ARQ_INDICE_GENERO);
    inicializar_arquivo_lista(ARQ_LISTA_GENERO);
}

// Cria uma ligação 1:N entre a string do Diretor e o ID do Filme
void inserir_indice_diretor(const char* diretor, int id_filme) {
    inicializar_indices_secundarios();

    FILE* f_idx   = fopen(ARQ_INDICE_DIRETOR, "rb+"); // abre índice p/ leitura+escrita
    FILE* f_lista = fopen(ARQ_LISTA_DIRETOR, "rb+");  // abre lista p/ leitura+escrita
    if (!f_idx || !f_lista) return;

    IndiceDiretor entrada;
    long offset_entrada = -1;
    bool encontrado = false;

    // Busca Sequencial (O(N)): Procura no arquivo de chaves se o diretor já existe
    rewind(f_idx);
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) { // percorre todas as entradas
        if (strcmp(entrada.diretor, diretor) == 0) {
            // Retrocede o cursor para guardar a posição física da struct no disco
            offset_entrada = ftell(f_idx) - (long)sizeof(IndiceDiretor);
            encontrado = true;
            break;
        }
    }

    // A inserção ocorre sempre no INÍCIO (cabeça) da lista encadeada (Tempo O(1)).
    // O novo nó apontará para quem era o "primeiro" anteriormente, e a struct IndiceDiretor
    // passará a apontar para esse novo nó gerado.
    long antigo_primeiro = encontrado ? entrada.primeiro : -1;

    NoLista novo_no;
    novo_no.id_filme = id_filme;
    novo_no.proximo  = antigo_primeiro;
    
    // Pede espaço no disco (reaproveitado ou no fim)
    long offset_novo_no = alocar_no_lista(f_lista, novo_no);

    if (encontrado) {
        // Atualiza a cabeça da lista de um diretor que já existia
        entrada.primeiro = offset_novo_no;
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceDiretor), 1, f_idx);
    } else {
        // Primeiro filme desse diretor. Cria o registro no final do arquivo de chaves.
        memset(entrada.diretor, 0, sizeof(entrada.diretor));
        strncpy(entrada.diretor, diretor, sizeof(entrada.diretor) - 1);
        entrada.primeiro = offset_novo_no;
        
        fseek(f_idx, 0, SEEK_END); // vai pro fim do arquivo
        fwrite(&entrada, sizeof(IndiceDiretor), 1, f_idx);
    }

    fclose(f_idx);
    fclose(f_lista);
}

// Navega nos ponteiros físicos em disco para resgatar todos os filmes de um diretor
vector<int> buscar_indice_diretor(const char* diretor) {
    vector<int> ids;

    FILE* f_idx   = fopen(ARQ_INDICE_DIRETOR, "rb"); // só leitura
    FILE* f_lista = fopen(ARQ_LISTA_DIRETOR, "rb");
    if (!f_idx || !f_lista) return ids;

    IndiceDiretor entrada;
    bool encontrado = false;
    
    // Busca a chave primária sequencialmente
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) {
        if (strcmp(entrada.diretor, diretor) == 0) {
            encontrado = true;
            break;
        }
    }

    // Navegação em Lista Encadeada no disco
    // Pula fisicamente de offset em offset até encontrar o marcador de fim (-1)
    if (encontrado) {
        long offset_atual = entrada.primeiro;
        while (offset_atual != -1) { // percorre a lista até o fim
            NoLista no;
            fseek(f_lista, offset_atual, SEEK_SET);
            fread(&no, sizeof(NoLista), 1, f_lista);
            
            ids.push_back(no.id_filme);
            offset_atual = no.proximo; // Vai para a posição de disco do próximo nó
        }
    }

    fclose(f_idx);
    fclose(f_lista);
    return ids;
}

// Remove uma relação da lista encadeada (necessário para updates e deletes)
void remover_indice_diretor(const char* diretor, int id_filme) {
    FILE* f_idx   = fopen(ARQ_INDICE_DIRETOR, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_DIRETOR, "rb+");
    if (!f_idx || !f_lista) return;

    IndiceDiretor entrada;
    long offset_entrada = -1;
    bool encontrado = false;
    
    // Localiza o bloco do diretor no disco
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) {
        if (strcmp(entrada.diretor, diretor) == 0) {
            offset_entrada = ftell(f_idx) - (long)sizeof(IndiceDiretor);
            encontrado = true;
            break;
        }
    }

    if (!encontrado) { fclose(f_idx); fclose(f_lista); return; } // diretor não cadastrado

    // Rastreamento para Exclusão em Lista Simplesmente Encadeada
    // Mantemos um offset_anterior. Quando achamos o nó alvo, fazemos o anterior
    // apontar para o 'proximo' do atual, descolando o atual da corrente.
    long offset_anterior = -1;
    long offset_atual = entrada.primeiro;
    NoLista no_atual;
    bool achou_no = false;

    while (offset_atual != -1) { // percorre a lista buscando o id_filme
        fseek(f_lista, offset_atual, SEEK_SET);
        fread(&no_atual, sizeof(NoLista), 1, f_lista);
        
        if (no_atual.id_filme == id_filme) { 
            achou_no = true; 
            break; 
        }
        
        offset_anterior = offset_atual;
        offset_atual = no_atual.proximo;
    }

    if (!achou_no) { fclose(f_idx); fclose(f_lista); return; } // filme não estava na lista

    // Costura dos ponteiros de disco
    if (offset_anterior == -1) {
        // Se o nó a remover era o primeiro da lista, alteramos a raiz da lista no arquivo de índices
        entrada.primeiro = no_atual.proximo;
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceDiretor), 1, f_idx);
    } else {
        // Se estava no meio, atualiza o nó anterior para pular o nó excluído
        NoLista no_anterior;
        fseek(f_lista, offset_anterior, SEEK_SET);
        fread(&no_anterior, sizeof(NoLista), 1, f_lista);
        
        no_anterior.proximo = no_atual.proximo;
        
        fseek(f_lista, offset_anterior, SEEK_SET);
        fwrite(&no_anterior, sizeof(NoLista), 1, f_lista);
    }

    // Libera o espaço deixado pelo nó para reaproveitamento futuro (envia para a LED)
    liberar_no_lista(f_lista, offset_atual);

    fclose(f_idx);
    fclose(f_lista);
}

// Cria uma ligação 1:N entre a string do Gênero e o ID do Filme (Idêntico à lógica de diretor)
void inserir_indice_genero(const char* genero, int id_filme) {
    inicializar_indices_secundarios();

    FILE* f_idx   = fopen(ARQ_INDICE_GENERO, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_GENERO, "rb+");
    if (!f_idx || !f_lista) return;

    IndiceGenero entrada;
    long offset_entrada = -1;
    bool encontrado = false;

    rewind(f_idx);
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) { // busca sequencial pelo gênero
        if (strcmp(entrada.genero, genero) == 0) {
            offset_entrada = ftell(f_idx) - (long)sizeof(IndiceGenero);
            encontrado = true;
            break;
        }
    }

    long antigo_primeiro = encontrado ? entrada.primeiro : -1;

    NoLista novo_no;
    novo_no.id_filme = id_filme;
    novo_no.proximo  = antigo_primeiro;
    long offset_novo_no = alocar_no_lista(f_lista, novo_no); // insere na cabeça

    if (encontrado) {
        entrada.primeiro = offset_novo_no;
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceGenero), 1, f_idx);
    } else {
        memset(entrada.genero, 0, sizeof(entrada.genero));
        strncpy(entrada.genero, genero, sizeof(entrada.genero) - 1);
        entrada.primeiro = offset_novo_no;
        fseek(f_idx, 0, SEEK_END);
        fwrite(&entrada, sizeof(IndiceGenero), 1, f_idx);
    }

    fclose(f_idx);
    fclose(f_lista);
}

// Navega nos ponteiros físicos em disco para resgatar todos os filmes de um gênero
vector<int> buscar_indice_genero(const char* genero) {
    vector<int> ids;

    FILE* f_idx   = fopen(ARQ_INDICE_GENERO, "rb");
    FILE* f_lista = fopen(ARQ_LISTA_GENERO, "rb");
    if (!f_idx || !f_lista) return ids;

    IndiceGenero entrada;
    bool encontrado = false;
    
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) { // busca a chave
        if (strcmp(entrada.genero, genero) == 0) {
            encontrado = true;
            break;
        }
    }

    if (encontrado) {
        long offset_atual = entrada.primeiro;
        while (offset_atual != -1) { // percorre a lista encadeada
            NoLista no;
            fseek(f_lista, offset_atual, SEEK_SET);
            fread(&no, sizeof(NoLista), 1, f_lista);
            ids.push_back(no.id_filme);
            offset_atual = no.proximo;
        }
    }

    fclose(f_idx);
    fclose(f_lista);
    return ids;
}

// Remove uma relação da lista encadeada de gênero
void remover_indice_genero(const char* genero, int id_filme) {
    FILE* f_idx   = fopen(ARQ_INDICE_GENERO, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_GENERO, "rb+");
    if (!f_idx || !f_lista) return;

    IndiceGenero entrada;
    long offset_entrada = -1;
    bool encontrado = false;
    
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) { // localiza o gênero
        if (strcmp(entrada.genero, genero) == 0) {
            offset_entrada = ftell(f_idx) - (long)sizeof(IndiceGenero);
            encontrado = true;
            break;
        }
    }

    if (!encontrado) { fclose(f_idx); fclose(f_lista); return; }

    long offset_anterior = -1;
    long offset_atual = entrada.primeiro;
    NoLista no_atual;
    bool achou_no = false;

    while (offset_atual != -1) { // busca o nó do id_filme na lista
        fseek(f_lista, offset_atual, SEEK_SET);
        fread(&no_atual, sizeof(NoLista), 1, f_lista);
        if (no_atual.id_filme == id_filme) { achou_no = true; break; }
        offset_anterior = offset_atual;
        offset_atual = no_atual.proximo;
    }

    if (!achou_no) { fclose(f_idx); fclose(f_lista); return; }

    if (offset_anterior == -1) {
        entrada.primeiro = no_atual.proximo; // removeu a cabeça
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceGenero), 1, f_idx);
    } else {
        NoLista no_anterior;
        fseek(f_lista, offset_anterior, SEEK_SET);
        fread(&no_anterior, sizeof(NoLista), 1, f_lista);
        no_anterior.proximo = no_atual.proximo; // pula o nó removido
        fseek(f_lista, offset_anterior, SEEK_SET);
        fwrite(&no_anterior, sizeof(NoLista), 1, f_lista);
    }

    liberar_no_lista(f_lista, offset_atual); // Devolve para a LED

    fclose(f_idx);
    fclose(f_lista);
}

// Utilidade para reconstruir os índices secundários varrendo o BD inteiro
// Extremamente útil em caso de falha física ou corrupção de arquivos.
void reconstruir_indices_secundarios() {
    // Exclui os arquivos velhos
    remove(ARQ_INDICE_DIRETOR);
    remove(ARQ_LISTA_DIRETOR);
    remove(ARQ_INDICE_GENERO);
    remove(ARQ_LISTA_GENERO);
    
    inicializar_indices_secundarios(); // recria vazios

    FILE* f_dados = fopen(ARQ_DADOS_FILMES, "rb"); // abre arquivo mestre
    if (!f_dados) {
        cout << "Arquivo filmes.dat nao encontrado. Nada para indexar.\n";
        return;
    }

    // Pula o cabeçalho do arquivo mestre de dados
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    
    Filme f;
    int total = 0;
    
    // Leitura linear do arquivo inteiro recadastrando no Índice
    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) { // percorre todos os filmes
        if (f.titulo[0] != '*') { // '*' indica exclusão lógica no arquivo principal. Ignora deletados.
            inserir_indice_diretor(f.diretor, f.id_filme);
            inserir_indice_genero(f.genero, f.id_filme);
            total++;
        }
    }
    fclose(f_dados);

    cout << "Indices reconstruidos com sucesso a partir de " << total
         << " filme(s) ativo(s) em filmes.dat.\n";
}

// Varre todo o arquivo de Índice Diretor printando a chave e buscando os valores na Lista Invertida
void exibir_indice_diretor() {
    FILE* f_idx = fopen(ARQ_INDICE_DIRETOR, "rb");
    if (!f_idx) { cout << "Indice de diretor ainda nao existe.\n"; return; }

    cout << "\n--- INDICE SECUNDARIO: DIRETOR ---\n";
    IndiceDiretor entrada;
    bool tem_algum = false;
    
    // Lista a tabela primária do índice.
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) { // percorre cada diretor
        tem_algum = true;
        // Para cada chave, navega e traz os IDs associados da lista invertida usando a função de busca.
        vector<int> ids = buscar_indice_diretor(entrada.diretor);
        
        cout << entrada.diretor << " -> [";
        for (size_t i = 0; i < ids.size(); i++) { // imprime os ids separados por vírgula
            cout << ids[i];
            if (i + 1 < ids.size()) cout << ", ";
        }
        cout << "]\n";
    }
    
    if (!tem_algum) cout << "Nenhum diretor indexado ainda.\n";
    fclose(f_idx);
}

// Varre todo o arquivo de Índice Gênero printando a chave e buscando os valores na Lista Invertida
void exibir_indice_genero() {
    FILE* f_idx = fopen(ARQ_INDICE_GENERO, "rb");
    if (!f_idx) { cout << "Indice de genero ainda nao existe.\n"; return; }

    cout << "\n--- INDICE SECUNDARIO: GENERO ---\n";
    IndiceGenero entrada;
    bool tem_algum = false;
    
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) { // percorre cada gênero
        tem_algum = true;
        vector<int> ids = buscar_indice_genero(entrada.genero);
        
        cout << entrada.genero << " -> [";
        for (size_t i = 0; i < ids.size(); i++) {
            cout << ids[i];
            if (i + 1 < ids.size()) cout << ", ";
        }
        cout << "]\n";
    }
    if (!tem_algum) cout << "Nenhum genero indexado ainda.\n";
    fclose(f_idx);
}

// Menu integrado para testes da estrutura no terminal
void menu_indices_secundarios() {
    int opcao;
    do {
        cout << "\n--- INDICES SECUNDARIOS ---\n";
        cout << "1 - Buscar filmes por diretor\n";
        cout << "2 - Buscar filmes por genero\n";
        cout << "3 - Exibir indice completo de diretores\n";
        cout << "4 - Exibir indice completo de generos\n";
        cout << "5 - Reconstruir indices a partir de filmes.dat\n";
        cout << "0 - Voltar\n";
        cout << "Escolha uma opcao: ";
        
        cin >> opcao;
        cin.ignore(); // limpa o \n do buffer
        
        if (opcao == 1) {
            char diretor[50];
            cout << "Diretor: ";
            cin.getline(diretor, 50);
            vector<int> ids = buscar_indice_diretor(diretor);
            if (ids.empty()) {
                cout << "Nenhum filme encontrado para esse diretor.\n";
            } else {
                cout << "Filmes encontrados: ";
                FILE* f_dados = fopen(ARQ_DADOS_FILMES, "rb");
                if (f_dados) {
                    fseek(f_dados, sizeof(Cabecalho), SEEK_SET); // pula cabeçalho
                    Filme f;
                    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
                        for (int id : ids) {
                            if (f.id_filme == id && f.titulo[0] != '*') {
                                cout << "\"" << f.titulo << "\" ";
                            }
                        }
                    }
                    fclose(f_dados);
                }
                cout << "\n";
            }
        } else if (opcao == 2) {
            char genero[30];
            cout << "Genero: ";
            cin.getline(genero, 30);
            vector<int> ids = buscar_indice_genero(genero);
            if (ids.empty()) {
                cout << "Nenhum filme encontrado para esse genero.\n";
            } else {
                cout << "Filmes encontrados: ";
                FILE* f_dados = fopen(ARQ_DADOS_FILMES, "rb");
                if (f_dados) {
                    fseek(f_dados, sizeof(Cabecalho), SEEK_SET); // pula cabeçalho
                    Filme f;
                    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
                        for (int id : ids) {
                            if (f.id_filme == id && f.titulo[0] != '*') {
                                cout << "\"" << f.titulo << "\" ";
                            }
                        }
                    }
                    fclose(f_dados);
                }
                cout << "\n";
            }
        } else if (opcao == 3) {
            exibir_indice_diretor();
        } else if (opcao == 4) {
            exibir_indice_genero();
        } else if (opcao == 5) {
            reconstruir_indices_secundarios();
        } else if (opcao != 0) {
            cout << "Opcao invalida!\n";
        }
    } while (opcao != 0); // repete até escolher sair
}

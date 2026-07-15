#include <iostream>
#include <fstream>
#include <cstring>
#include <cstddef>
#include "../include/filme.h"
#include "../include/led.h"
#include "../include/arvoreb.h"
#include "../include/indices.h"
 
using namespace std;

// O arquivo principal simula o disco físico. 
// Todos os registros aqui gravados possuem tamanho fixo (definido pela struct Filme), 
// o que permite que façamos saltos com fseek calculando a posição exata em bytes.
const char* ARQUIVO_DADOS = "dados/Filmes.dat";

// Função para garantir que o arquivo e o cabeçalho existam
void inicializar_arquivo_se_nao_existir() {
    FILE* f = fopen(ARQUIVO_DADOS, "rb");
    if (f == NULL) {
        f = fopen(ARQUIVO_DADOS, "wb+");
        if (f != NULL) {
            Cabecalho cab;
            cab.proximo_id = 1;
            fwrite(&cab, sizeof(Cabecalho), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}

// Operação de cadastro

void cadastrar_filme() {
    inicializar_arquivo_se_nao_existir();

    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb+");
    if (!f_dados) {
        cout << "Erro ao abrir o arquivo de dados.\n";
        return;
    }

    // Cabeçalho persistido.
    // Lemos os metadados no início do arquivo para obter o próximo ID autoincrementado.
    Cabecalho cab;
    rewind(f_dados); 
    fread(&cab, sizeof(Cabecalho), 1, f_dados);

    // Configurar o filme com o ID gerado
    Filme f;
    f.id_filme = cab.proximo_id;

    cout << "Digite o Titulo do Filme: ";
    cin.getline(f.titulo, 100);
    cout << "Diretor: ";
    cin.getline(f.diretor, 50);
    cout << "Genero: ";
    cin.getline(f.genero, 30);
    cout << "Duracao (minutos): ";
    cin >> f.duracao;
    cout << "Nota (0.0 a 10.0): ";
    cin >> f.nota;
    cin.ignore();
    cout << "Opiniao/Comentario: ";
    cin.getline(f.opiniao, 300);

    // Reaproveitamento da LED
    // Antes de expandir o arquivo (Append), o sistema consulta se há offsets na Lista de Espaços Disponíveis.
    // Se 'obter_espaco_livre()' retornar um offset válido (diferente de -1), o registro será gravado por cima 
    // de um registro logicamente excluído, combatendo a fragmentação física do arquivo.
    long target_offset = obter_espaco_livre();

    if (target_offset == -1) {
        // Nao havia espaco livre reaproveitavel: insere no final do arquivo (Append)
        fseek(f_dados, 0, SEEK_END);
        target_offset = ftell(f_dados);
    }

    // Move o ponteiro físico diretamente para o offset de destino para escrever a struct de tamanho fixo
    fseek(f_dados, target_offset, SEEK_SET);
    fwrite(&f, sizeof(Filme), 1, f_dados);

    // Atualizar o próximo ID sequencial no cabeçalho
    cab.proximo_id++;
    rewind(f_dados);
    fwrite(&cab, sizeof(Cabecalho), 1, f_dados);

    fclose(f_dados);

    cout << "Filme gravado com sucesso no offset: " << target_offset << "!\n";

    // Sistema multiarquivo
    // Após escrever no arquivo de dados principal, atualizamos de forma independente o índice primário (Árvore B+)
    // e os dois índices secundários (Diretor e Gênero) persistidos em disco.
    inserir_arvore(f.id_filme, target_offset);
    inserir_indice_diretor(f.diretor, f.id_filme);
    inserir_indice_genero(f.genero, f.id_filme);
}

// operação em busca

void buscar_filme() {
    cout << "\n--- BUSCAR FILME ---\n";
    cout << "1. Buscar por ID\n";
    cout << "2. Buscar por Diretor/Genero\n";
    int opcao;
    cin >> opcao;
    cin.ignore();

    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb");
    if (!f_dados) {
        cout << "Nenhum arquivo de dados encontrado.\n";
        return;
    }

    // Pulando o cabeçalho
    // Usamos fseek para pular os bytes do cabeçalho de metadados e iniciar a busca a partir 
    // do primeiro registro real de dados de filme.
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    Filme f;
    bool encontrou = false;

    if (opcao == 1) {
        int id;
        cout << "Digite o ID do Filme: ";
        cin >> id;

        // Verificação de Exclusão Lógica.
        // Ao ler sequencialmente, o primeiro byte de cada registro é verificado.
        // Se for '*', o registro está logicamente excluído (está na LED), logo deve ser ignorado na busca.
        while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
            if (f.titulo[0] != '*' && f.id_filme == id) {
                cout << "\nFilme Encontrado!\n";
                cout << "ID: " << f.id_filme << " | Titulo: " << f.titulo << " | Diretor: " << f.diretor << "\n";
                encontrou = true;
                break;
            }
        }
    } else {
        char chave[100];
        cout << "Digite o termo de busca: ";
        cin.getline(chave, 100);

        while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
            if (f.titulo[0] != '*') {
                bool corresponde = (opcao == 2) ? (strcmp(f.diretor, chave) == 0) : (strcmp(f.genero, chave) == 0);
                if (corresponde) {
                    cout << "ID: " << f.id_filme << " | Titulo: " << f.titulo << " | Diretor: " << f.diretor << "\n";
                    encontrou = true;
                }
            }
        }
    }
    if (!encontrou) cout << "Nenhum registro correspondente foi encontrado.\n";
    fclose(f_dados);
}

// operação de atualização

void atualizar_filme() {
    cout << "\n--- ATUALIZAR FILME ---\n";
    int id;
    cout << "Digite o ID do filme a atualizar: ";
    cin >> id;
    cin.ignore();

    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb+");
    if (!f_dados) {
        cout << "Arquivo de dados nao encontrado.\n";
        return;
    }

    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    Filme f;
    long offset_atual = -1;
    bool encontrou = false;

    // Varre o disco guardando dinamicamente a posição física atual (offset_atual) através de ftell
    while (true) {
        offset_atual = ftell(f_dados);
        if (fread(&f, sizeof(Filme), 1, f_dados) != 1) break;

        if (f.titulo[0] != '*' && f.id_filme == id) {
            encontrou = true;
            break;
        }
    }

    if (!encontrou) {
        cout << "Filme nao encontrado.\n";
        fclose(f_dados);
        return;
    }

    // Atualização dos indices secundários
    // Como os índices secundários são baseados em campos que podem ser alterados (Diretor/Gênero),
    // precisamos armazenar seus valores originais antes que o usuário digite os novos.
    // Se eles mudarem, removemos o ID da lista invertida antiga e inserimos na nova lista correspondente.
    char diretor_antigo[50];
    char genero_antigo[30];
    strncpy(diretor_antigo, f.diretor, sizeof(diretor_antigo));
    strncpy(genero_antigo, f.genero, sizeof(genero_antigo));
 
    cout << "Novo Titulo (Atual: " << f.titulo << "): ";
    cin.getline(f.titulo, 100);
    cout << "Novo Diretor (Atual: " << f.diretor << "): ";
    cin.getline(f.diretor, 50);
    cout << "Novo Genero (Atual: " << f.genero << "): ";
    cin.getline(f.genero, 30);
    cout << "Nova Nota: ";
    cin >> f.nota;
    cin.ignore();

    // Escrita in-place
    // Usamos fseek para voltar exatamente para a posição 'offset_atual' de onde lemos este registro, 
    // e regravamos a struct atualizada mantendo exatamente o mesmo espaço físico em disco.
    fseek(f_dados, offset_atual, SEEK_SET);
    fwrite(&f, sizeof(Filme), 1, f_dados);
    fclose(f_dados);

    // Se diretor/genero mudaram, religa as entradas nos indices secundarios
    if (strcmp(diretor_antigo, f.diretor) != 0) {
        remover_indice_diretor(diretor_antigo, f.id_filme);
        inserir_indice_diretor(f.diretor, f.id_filme);
    }
    if (strcmp(genero_antigo, f.genero) != 0) {
        remover_indice_genero(genero_antigo, f.id_filme);
        inserir_indice_genero(f.genero, f.id_filme);
    }
 
    cout << "Filme atualizado com sucesso!\n";
}

// operação de remoção

void remover_filme() {
    cout << "\n--- REMOVER FILME (EXCLUSÃO LÓGICA) ---\n";
    int id;
    cout << "Digite o ID do filme a remover: ";
    cin >> id;

    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb+");
    if (!f_dados) {
        cout << "Arquivo de dados nao encontrado.\n";
        return;
    }

    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    Filme f;
    long offset_filme = -1;
    bool encontrou = false;

    // Localiza o filme para descobrir em qual offset físico ele está
    while (true) {
        offset_filme = ftell(f_dados);
        if (fread(&f, sizeof(Filme), 1, f_dados) != 1) break;

        if (f.titulo[0] != '*' && f.id_filme == id) {
            encontrou = true;
            break;
        }
    }

    if (!encontrou) {
        cout << "Filme nao encontrado.\n";
        fclose(f_dados);
        return;
    }

    // Exclusão lógica e LED
    // Como exigido pelas especificações do trabalho (LED), a remoção não compacta o arquivo imediatamente.
    // Nós voltamos para o início do registro em disco (fseek para offset_filme) e sobrescrevemos APENAS 
    // o primeiro byte do campo título com o caractere asterisco '*'.
    long offset_titulo = offset_filme + offsetof(Filme, titulo);
    fseek(f_dados, offset_titulo, SEEK_SET);

    char marcador = '*';
    fwrite(&marcador, sizeof(char), 1, f_dados); // Agora sim gravamos o '*' no byte correto!

    //  Manutenção do Ecossistema Multi-Arquivos.
    // Após marcar o registro principal como apagado logicamente:
    // 1. Cadastramos seu offset em 'LED.dat' para reuso futuro.
    // 2. Removemos do índice primário (Árvore B+).
    // 3. Removemos das listas secundárias invertidas de Diretor e Gênero.
    inserir_na_led(offset_filme);

    remover_arvore(id);

    remover_indice_diretor(f.diretor, id);
    remover_indice_genero(f.genero, id);
 
    fclose(f_dados);

    cout << "Filme com ID " << id << " removido e adicionado a LED com sucesso.\n";
}

// Exibe na tela todos os registros fisicamente ativos em disco
void listar_filme() {
    cout << "\n--- LISTAGEM DE FILMES ATIVOS EM DISCO ---\n";
    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb");
    if (!f_dados) {
        cout << "Nenhum arquivo de dados encontrado.\n";
        return;
    }

    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);

    Filme f;
    bool encontrou = false;

    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
        // Ignora exibições de dados cuja marcação inicial seja o marcador lógico '*'
        if (f.titulo[0] != '*') {
            cout << "ID: " << f.id_filme 
                 << " | Titulo: " << f.titulo 
                 << " | Diretor: " << f.diretor 
                 << " | Genero: " << f.genero 
                 << " | Nota: " << f.nota << "\n";
            encontrou = true;
        }
    }

    if (!encontrou) {
        cout << "Nenhum filme ativo cadastrado no momento.\n";
    }    
    fclose(f_dados);
}

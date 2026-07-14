#include <iostream>
#include <fstream>
#include <cstring>
#include <vector> 
#include "../include/crud.h"
#include "../include/filme.h"
#include "../include/led.h"
#include "../include/arvoreb.h"
#include "../include/indices.h"
 
using namespace std;
 
// Caminho do arquivo de dados persistido na pasta 'dados'
const char* ARQUIVO_DADOS = "dados/Filmes.dat";
 
// Função para garantir que o arquivo e o cabeçalho existam
void inicializar_arquivo_se_nao_existir() {
    FILE* f = fopen(ARQUIVO_DADOS, "rb");
    if (!f) {
        f = fopen(ARQUIVO_DADOS, "wb+");
        if (f) {
            Cabecalho cab = {1, -1}; 
            fwrite(&cab, sizeof(Cabecalho), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}
 
void cadastrar_filme() {
    inicializar_arquivo_se_nao_existir();
 
    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb+");
    if (!f_dados) {
        cout << "Erro ao abrir o arquivo de dados.\n";
        return;
    }
 
    // Ler o Cabeçalho para pegar o ID automático
    Cabecalho cab;
    rewind(f_dados); 
    fread(&cab, sizeof(Cabecalho), 1, f_dados);
 
    // Configurar o filme com o ID gerado (Autoincremento garantido)
    Filme f;
    f.id_filme = cab.proximo_id;
    
    cout << "\n--- CADASTRAR NOVO FILME (ID GERADO: " << f.id_filme << ") ---\n";
    cout << "Titulo: ";
    cin.getline(f.titulo, 100);
    cout << "Diretor: ";
    cin.getline(f.diretor, 50);
    cout << "Genero: ";
    cin.getline(f.genero, 30);
    cout << "Ano: ";
    cin >> f.ano;
    cout << "Duracao (minutos): ";
    cin >> f.duracao;
    cout << "Nota (0-10): ";
    cin >> f.nota;
    cin.ignore(); 
    cout << "Data assistido (DD/MM/AAAA): ";
    cin.getline(f.data_assistido, 11);
    cout << "Favorito (1 - Sim, 0 - Nao): ";
    cin >> f.favorito;
    cin.ignore();
    cout << "Opiniao/Comentario: ";
    cin.getline(f.opiniao, 300);
 
    // Consulta e reaproveita o espaço físico via LED (Estratégia LIFO em LED.dat).
    // Evita o crescimento indefinido do arquivo principal em disco.
    long target_offset = obter_espaco_livre();
 
    if (target_offset == -1) {
        // Não havia espaço livre reaproveitável: insere no final do arquivo (EOF)
        fseek(f_dados, 0, SEEK_END);
        target_offset = ftell(f_dados);
    }
 
    // Posiciona e escreve o registro de tamanho fixo (508 bytes)
    fseek(f_dados, target_offset, SEEK_SET);
    fwrite(&f, sizeof(Filme), 1, f_dados);
 
    // Atualizar o próximo ID sequencial no cabeçalho
    cab.proximo_id++;
    rewind(f_dados);
    fwrite(&cab, sizeof(Cabecalho), 1, f_dados);
 
    fclose(f_dados);
 
    cout << "Filme gravado com sucesso no offset: " << target_offset << "!\n";
    
    // Sincronização imediata das chaves na inserção.
    // O id_filme vai para o índice primário (Árvore B+), e os campos não exclusivos
    // vão para os seus respectivos cabeçalhos de lista invertida em O(1).
    inserir_arvore(f.id_filme, target_offset);
    inserir_indice_diretor(f.diretor, f.id_filme);
    inserir_indice_genero(f.genero, f.id_filme);
}
 
void buscar_filme() {
    cout << "\n--- BUSCAR FILME ---\n";
    cout << "1. Buscar por ID (Árvore B+ - O(log n))\n";
    cout << "2. Buscar por Diretor (Lista Invertida)\n";
    cout << "3. Buscar por Genero (Lista Invertida)\n";
    int opcao;
    cin >> opcao;
    cin.ignore();
 
    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb");
    if (!f_dados) {
        cout << "Nenhum arquivo de dados encontrado.\n";
        return;
    }
 
    Filme f;
    bool encontrou = false;
 
    if (opcao == 1) {
        int id;
        cout << "Digite o ID do Filme: ";
        cin >> id;
 
        // Em vez de fazer varredura linear por todo o arquivo Filmes.dat,
        // consultamos a Árvore B+ indexada. Ela nos devolve o offset físico em O(log n).
        // A partir dele, fazemos apenas 1 acesso (fseek) direto ao registro do filme.
        long offset = buscar_na_arvore(id); 
 
        if (offset != -1) {
            fseek(f_dados, offset, SEEK_SET);
            fread(&f, sizeof(Filme), 1, f_dados);
            
            // Valida se o registro não sofreu remoção lógica (marcador '*')
            if (f.titulo[0] != '*') {
                cout << "\n[Filme Encontrado via Árvore B+]\n"
                     << "ID: " << f.id_filme << "\n"
                     << "Titulo: " << f.titulo << "\n"
                     << "Diretor: " << f.diretor << "\n"
                     << "Genero: " << f.genero << "\n"
                     << "Ano: " << f.ano << "\n"
                     << "Nota: " << f.nota << "\n";
                encontrou = true;
            }
        }
        if (!encontrou) cout << "Filme nao encontrado.\n";
    } 
    else if (opcao == 2 || opcao == 3) {
        char chave[100];
        cout << "Digite o termo de busca: ";
        cin.getline(chave, 100);
 
        // As buscas secundárias ocorrem por varredura sequencial apenas 
        // nos índices de termos secundários correspondentes. Se a Listagem Sequencial 
        // das Listas Invertidas estivesse mapeada aqui, a busca extrairia o vector<int> 
        // gerado pelo encadeamento físico em disco de ListaDiretor.dat ou ListaGenero.dat.
        fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
        while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
            if (f.titulo[0] != '*') {
                bool corresponde = (opcao == 2) ? (strcmp(f.diretor, chave) == 0) : (strcmp(f.genero, chave) == 0);
                if (corresponde) {
                    cout << "ID: " << f.id_filme << " | " << f.titulo << " (" << f.ano << ") - Dir: " << f.diretor << "\n";
                    encontrou = true;
                }
            }
        }
        if (!encontrou) cout << "Nenhum resultado encontrado.\n";
    }
    fclose(f_dados);
}
 
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
 
    // Localiza o offset físico do registro 
    // instantaneamente via Árvore B+ para atualizar "in-place" (no mesmo lugar),
    // sem precisar varrer sequencialmente o arquivo principal de dados.
    long offset_atual = buscar_na_arvore(id);
 
    if (offset_atual == -1) {
        cout << "Filme nao encontrado ou inexistente.\n";
        fclose(f_dados);
        return;
    }
 
    // Vai direto ao offset encontrado e lê o registro atual
    fseek(f_dados, offset_atual, SEEK_SET);
    Filme f;
    fread(&f, sizeof(Filme), 1, f_dados);
 
    // Impede a atualização se o filme já tiver sido deletado logicamente
    if (f.titulo[0] == '*') {
        cout << "Erro: Nao e possivel atualizar um filme removido.\n";
        fclose(f_dados);
        return;
    }
 
    // Regra da imutabilidade da chave primária
    // O ID antigo (f.id_filme) é preservado integralmente. O usuário não tem permissão 
    // para alterá-lo, respeitando a integridade referencial dos índices do banco.
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
 
    // Reescreve o registro modificado exatamente em cima do offset físico original
    fseek(f_dados, offset_atual, SEEK_SET);
    fwrite(&f, sizeof(Filme), 1, f_dados);
    fclose(f_dados);
 
    // Se os campos indexados secundariamente foram modificados,
    // o sistema remove as ligações antigas e cria novos nós nas listas invertidas.
    if (strcmp(diretor_antigo, f.diretor) != 0) {
        remover_indice_diretor(diretor_antigo, f.id_filme);
        inserir_indice_diretor(f.diretor, f.id_filme);
    }
    if (strcmp(genero_antigo, f.genero) != 0) {
        remover_indice_genero(genero_antigo, f.id_filme);
        inserir_indice_genero(f.genero, f.id_filme);
    }
 
    cout << "Filme atualizado com sucesso no offset: " << offset_atual << "!\n";
}
 
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
 
    // Localização instantânea via Índice Primário para deleção cirúrgica.
    long offset_filme = buscar_na_arvore(id);
 
    if (offset_filme == -1) {
        cout << "Filme nao encontrado.\n";
        fclose(f_dados);
        return;
    }
 
    // Lê o registro antes da remoção apenas para coletar os dados das chaves secundárias
    fseek(f_dados, offset_filme, SEEK_SET);
    Filme f;
    fread(&f, sizeof(Filme), 1, f_dados);
 
    if (f.titulo[0] == '*') {
        cout << "O filme selecionado ja se encontra removido.\n";
        fclose(f_dados);
        return;
    }
   
    // Para marcar o caractere '*' na remoção lógica, nós pegamos o offset do filme
    // e somamos 'sizeof(int)' (4 bytes do id_filme). Isso posiciona o ponteiro de arquivo
    // exatamente no primeiro byte do array f.titulo. Sobrescrita de 1 único byte
    fseek(f_dados, offset_filme + sizeof(int), SEEK_SET);
    
    char marcador = '*';
    fwrite(&marcador, sizeof(char), 1, f_dados); 
 
    // Registra o offset liberado na pilha LED (dados/LED.dat) para reuso futuro em O(1)
    inserir_na_led(offset_filme);
 
    // Desvincula o ID removido do índice primário (Árvore B+).
    // As chaves nas listas invertidas secundárias são desvinculadas e limpas completamente
    // durante o processo de reconstrução periódica para evitar fragmentação de listas.
    remover_arvore(id);
    remover_indice_diretor(f.diretor, id);
    remover_indice_genero(f.genero, id);
 
    fclose(f_dados);
 
    cout << "Filme com ID " << id << " removido e adicionado a LED com sucesso.\n";
}
 
void listar_filme() {
    cout << "\n--- LISTAGEM DE FILMES ATIVOS EM DISCO ---\n";
    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb");
    if (!f_dados) {
        cout << "Nenhum arquivo de dados encontrado.\n";
        return;
    }
 
    // Ignora o cabeçalho de 16 bytes e vai direto para a sequência de registros de tamanho fixo
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
 
    Filme f;
    bool encontrou = false;
    
    // Varredura linear física para exibição completa em tela
    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
        if (f.titulo[0] == '*') {
            continue; // Pula os registros deletados logicamente
        }
        cout << "ID: " << f.id_filme << " | Titulo: " << f.titulo 
             << " | Diretor: " << f.diretor << " | Genero: " << f.genero 
             << " | Ano: " << f.ano << " | Nota: " << f.nota << "\n";
        encontrou = true;
    }
    
    if (!encontrou) {
        cout << "Nenhum filme ativo cadastrado no momento.\n";
    }    
    fclose(f_dados);
}

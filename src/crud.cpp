
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
 
    // Configurar o filme com o ID gerado
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
 
    // Tenta reaproveitar um espaco livre da LED antes de crescer o arquivo
    long target_offset = obter_espaco_livre();
 
    if (target_offset == -1) {
        // Nao havia espaco livre reaproveitavel: insere no final do arquivo
        fseek(f_dados, 0, SEEK_END);
        target_offset = ftell(f_dados);
    }
 
    fseek(f_dados, target_offset, SEEK_SET);
    fwrite(&f, sizeof(Filme), 1, f_dados);
 
    // Atualizar o próximo ID no cabeçalho
    cab.proximo_id++;
    rewind(f_dados);
    fwrite(&cab, sizeof(Cabecalho), 1, f_dados);
 
    fclose(f_dados);
 
    cout << "Filme gravado com sucesso no offset: " << target_offset << "!\n";
    
    inserir_arvore(f.id_filme, target_offset);
    inserir_indice_diretor(f.diretor, f.id_filme);
    inserir_indice_genero(f.genero, f.id_filme);
}
 
void buscar_filme() {
    cout << "\n--- BUSCAR FILME ---\n";
    cout << "1. Buscar por ID\n";
    cout << "2. Buscar por Diretor\n";
    cout << "3. Buscar por Genero\n";
    int opcao;
    cin >> opcao;
    cin.ignore();
 
    FILE* f_dados = fopen(ARQUIVO_DADOS, "rb");
    if (!f_dados) {
        cout << "Nenhum arquivo de dados encontrado.\n";
        return;
    }
 
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    Filme f;
    bool encontrou = false;
 
    if (opcao == 1) {
        int id;
        cout << "Digite o ID do Filme: ";
        cin >> id;
 
        while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
            // Se o primeiro byte do título for '*', ignoramos porque está deletado
            if (f.titulo[0] != '*' && f.id_filme == id) {
                cout << "\n[Filme Encontrado]\nID: " << f.id_filme << "\nTitulo: " << f.titulo 
                     << "\nDiretor: " << f.diretor << "\nGenero: " << f.genero << "\nAno: " << f.ano << "\n";
                encontrou = true;
                break; 
            }
        }
        if (!encontrou) cout << "Filme nao encontrado.\n";
    } 
    else if (opcao == 2 || opcao == 3) {
        char chave[100];
        cout << "Digite o termo de busca: ";
        cin.getline(chave, 100);
 
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
 
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    Filme f;
    long offset_atual = -1;
    bool encontrou = false;
 
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
 
    // Guarda diretor/genero antigos para poder atualizar os indices secundarios depois
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
 
    // Reescreve o registro atualizado exatamente em cima do offset antigo
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
 
    // Localiza o filme para descobrir em qual offset ele está
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
    
    // Avança o ponteiro passando pelo id_filme (sizeof(int)) para apontar exatamente para f.titulo[0]
    fseek(f_dados, offset_filme + sizeof(int), SEEK_SET);
    
    char marcador = '*';
    fwrite(&marcador, sizeof(char), 1, f_dados); // Agora sim gravamos o '*' no byte correto!
 
    // Registra o offset liberado na LED (dados/LED.dat) para reaproveitamento futuro
    inserir_na_led(offset_filme);
 
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
 
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
 
    Filme f;
    bool encontrou = false;
    
    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
        if (f.titulo[0] == '*') {
            continue; 
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
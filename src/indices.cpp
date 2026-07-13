#include <iostream>
#include <cstring>
#include <cstdio>
#include "../include/indices.h"
 
using namespace std;
 
static const char* ARQ_INDICE_DIRETOR = "dados/IndiceDiretor.dat";
static const char* ARQ_LISTA_DIRETOR  = "dados/ListaDiretor.dat";
static const char* ARQ_INDICE_GENERO  = "dados/IndiceGenero.dat";
static const char* ARQ_LISTA_GENERO   = "dados/ListaGenero.dat";
static const char* ARQ_DADOS_FILMES   = "dados/Filmes.dat";
 
static long alocar_no_lista(FILE* f_lista, NoLista novo_no) {
    CabecalhoLista cab;
    rewind(f_lista);
    fread(&cab, sizeof(CabecalhoLista), 1, f_lista);
 
    long offset_no;
    if (cab.topo_led != -1) {
        offset_no = cab.topo_led;
        NoLista no_livre;
        fseek(f_lista, offset_no, SEEK_SET);
        fread(&no_livre, sizeof(NoLista), 1, f_lista);
        cab.topo_led = no_livre.proximo; //Próxima posição livre da LED
    } else {
        fseek(f_lista, 0, SEEK_END);
        offset_no = ftell(f_lista);
    }
 
    fseek(f_lista, offset_no, SEEK_SET);
    fwrite(&novo_no, sizeof(NoLista), 1, f_lista);
 
    rewind(f_lista);
    fwrite(&cab, sizeof(CabecalhoLista), 1, f_lista);
 
    return offset_no;
}
 
//Marca um nó como livre e o empilha no topo da LED
static void liberar_no_lista(FILE* f_lista, long offset_no) {
    CabecalhoLista cab;
    rewind(f_lista);
    fread(&cab, sizeof(CabecalhoLista), 1, f_lista);
 
    NoLista no_livre;
    no_livre.id_filme = -1;
    no_livre.proximo  = cab.topo_led;
 
    fseek(f_lista, offset_no, SEEK_SET);
    fwrite(&no_livre, sizeof(NoLista), 1, f_lista);
 
    cab.topo_led = offset_no;
    rewind(f_lista);
    fwrite(&cab, sizeof(CabecalhoLista), 1, f_lista);
}
 
static void inicializar_arquivo_indice(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) {
        f = fopen(caminho, "wb");
        if (f) fclose(f);
    } else {
        fclose(f);
    }
}
 
static void inicializar_arquivo_lista(const char* caminho) {
    FILE* f = fopen(caminho, "rb");
    if (!f) {
        f = fopen(caminho, "wb+");
        if (f) {
            CabecalhoLista cab;
            cab.topo_led = -1;
            fwrite(&cab, sizeof(CabecalhoLista), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}
 
void inicializar_indices_secundarios() {
    inicializar_arquivo_indice(ARQ_INDICE_DIRETOR);
    inicializar_arquivo_lista(ARQ_LISTA_DIRETOR);
    inicializar_arquivo_indice(ARQ_INDICE_GENERO);
    inicializar_arquivo_lista(ARQ_LISTA_GENERO);
}
 
void inserir_indice_diretor(const char* diretor, int id_filme) {
    inicializar_indices_secundarios();
 
    FILE* f_idx   = fopen(ARQ_INDICE_DIRETOR, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_DIRETOR, "rb+");
    if (!f_idx || !f_lista) {
        cout << "Erro ao abrir arquivos do indice de diretor.\n";
        if (f_idx) fclose(f_idx);
        if (f_lista) fclose(f_lista);
        return;
    }
 
    IndiceDiretor entrada;
    long offset_entrada = -1;
    bool encontrado = false;
 
    rewind(f_idx);
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) {
        if (strcmp(entrada.diretor, diretor) == 0) {
            offset_entrada = ftell(f_idx) - (long)sizeof(IndiceDiretor);
            encontrado = true;
            break;
        }
    }
 
    long antigo_primeiro = encontrado ? entrada.primeiro : -1;
 
    NoLista novo_no;
    novo_no.id_filme = id_filme;
    novo_no.proximo  = antigo_primeiro;
    long offset_novo_no = alocar_no_lista(f_lista, novo_no);
 
    if (encontrado) {
        entrada.primeiro = offset_novo_no;
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceDiretor), 1, f_idx);
    } else {
        memset(entrada.diretor, 0, sizeof(entrada.diretor));
        strncpy(entrada.diretor, diretor, sizeof(entrada.diretor) - 1);
        entrada.primeiro = offset_novo_no;
        fseek(f_idx, 0, SEEK_END);
        fwrite(&entrada, sizeof(IndiceDiretor), 1, f_idx);
    }
 
    fclose(f_idx);
    fclose(f_lista);
}
 
vector<int> buscar_indice_diretor(const char* diretor) {
    vector<int> ids;
 
    FILE* f_idx   = fopen(ARQ_INDICE_DIRETOR, "rb");
    FILE* f_lista = fopen(ARQ_LISTA_DIRETOR, "rb");
    if (!f_idx || !f_lista) {
        if (f_idx) fclose(f_idx);
        if (f_lista) fclose(f_lista);
        return ids;
    }
 
    IndiceDiretor entrada;
    bool encontrado = false;
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) {
        if (strcmp(entrada.diretor, diretor) == 0) {
            encontrado = true;
            break;
        }
    }
 
    if (encontrado) {
        long offset_atual = entrada.primeiro;
        while (offset_atual != -1) {
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
 
void remover_indice_diretor(const char* diretor, int id_filme) {
    FILE* f_idx   = fopen(ARQ_INDICE_DIRETOR, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_DIRETOR, "rb+");
    if (!f_idx || !f_lista) {
        if (f_idx) fclose(f_idx);
        if (f_lista) fclose(f_lista);
        return;
    }
 
    IndiceDiretor entrada;
    long offset_entrada = -1;
    bool encontrado = false;
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) {
        if (strcmp(entrada.diretor, diretor) == 0) {
            offset_entrada = ftell(f_idx) - (long)sizeof(IndiceDiretor);
            encontrado = true;
            break;
        }
    }
 
    if (!encontrado) { fclose(f_idx); fclose(f_lista); return; }
 
    //Percorre guardando o offset do nó anterior, para religar a lista depois
    long offset_anterior = -1;
    long offset_atual = entrada.primeiro;
    NoLista no_atual;
    bool achou_no = false;
 
    while (offset_atual != -1) {
        fseek(f_lista, offset_atual, SEEK_SET);
        fread(&no_atual, sizeof(NoLista), 1, f_lista);
        if (no_atual.id_filme == id_filme) { achou_no = true; break; }
        offset_anterior = offset_atual;
        offset_atual = no_atual.proximo;
    }
 
    if (!achou_no) { fclose(f_idx); fclose(f_lista); return; }
 
    if (offset_anterior == -1) {
        entrada.primeiro = no_atual.proximo;
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceDiretor), 1, f_idx);
    } else {
        NoLista no_anterior;
        fseek(f_lista, offset_anterior, SEEK_SET);
        fread(&no_anterior, sizeof(NoLista), 1, f_lista);
        no_anterior.proximo = no_atual.proximo;
        fseek(f_lista, offset_anterior, SEEK_SET);
        fwrite(&no_anterior, sizeof(NoLista), 1, f_lista);
    }
 
    liberar_no_lista(f_lista, offset_atual);
 
    fclose(f_idx);
    fclose(f_lista);
}
 
void inserir_indice_genero(const char* genero, int id_filme) {
    inicializar_indices_secundarios();
 
    FILE* f_idx   = fopen(ARQ_INDICE_GENERO, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_GENERO, "rb+");
    if (!f_idx || !f_lista) {
        cout << "Erro ao abrir arquivos do indice de genero.\n";
        if (f_idx) fclose(f_idx);
        if (f_lista) fclose(f_lista);
        return;
    }
 
    IndiceGenero entrada;
    long offset_entrada = -1;
    bool encontrado = false;
 
    rewind(f_idx);
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) {
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
    long offset_novo_no = alocar_no_lista(f_lista, novo_no);
 
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
 
vector<int> buscar_indice_genero(const char* genero) {
    vector<int> ids;
 
    FILE* f_idx   = fopen(ARQ_INDICE_GENERO, "rb");
    FILE* f_lista = fopen(ARQ_LISTA_GENERO, "rb");
    if (!f_idx || !f_lista) {
        if (f_idx) fclose(f_idx);
        if (f_lista) fclose(f_lista);
        return ids;
    }
 
    IndiceGenero entrada;
    bool encontrado = false;
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) {
        if (strcmp(entrada.genero, genero) == 0) {
            encontrado = true;
            break;
        }
    }
 
    if (encontrado) {
        long offset_atual = entrada.primeiro;
        while (offset_atual != -1) {
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
 
void remover_indice_genero(const char* genero, int id_filme) {
    FILE* f_idx   = fopen(ARQ_INDICE_GENERO, "rb+");
    FILE* f_lista = fopen(ARQ_LISTA_GENERO, "rb+");
    if (!f_idx || !f_lista) {
        if (f_idx) fclose(f_idx);
        if (f_lista) fclose(f_lista);
        return;
    }
 
    IndiceGenero entrada;
    long offset_entrada = -1;
    bool encontrado = false;
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) {
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
 
    while (offset_atual != -1) {
        fseek(f_lista, offset_atual, SEEK_SET);
        fread(&no_atual, sizeof(NoLista), 1, f_lista);
        if (no_atual.id_filme == id_filme) { achou_no = true; break; }
        offset_anterior = offset_atual;
        offset_atual = no_atual.proximo;
    }
 
    if (!achou_no) { fclose(f_idx); fclose(f_lista); return; }
 
    if (offset_anterior == -1) {
        entrada.primeiro = no_atual.proximo;
        fseek(f_idx, offset_entrada, SEEK_SET);
        fwrite(&entrada, sizeof(IndiceGenero), 1, f_idx);
    } else {
        NoLista no_anterior;
        fseek(f_lista, offset_anterior, SEEK_SET);
        fread(&no_anterior, sizeof(NoLista), 1, f_lista);
        no_anterior.proximo = no_atual.proximo;
        fseek(f_lista, offset_anterior, SEEK_SET);
        fwrite(&no_anterior, sizeof(NoLista), 1, f_lista);
    }
 
    liberar_no_lista(f_lista, offset_atual);
 
    fclose(f_idx);
    fclose(f_lista);
}
 
 
void reconstruir_indices_secundarios() {
    remove(ARQ_INDICE_DIRETOR);
    remove(ARQ_LISTA_DIRETOR);
    remove(ARQ_INDICE_GENERO);
    remove(ARQ_LISTA_GENERO);
    inicializar_indices_secundarios();
 
    FILE* f_dados = fopen(ARQ_DADOS_FILMES, "rb");
    if (!f_dados) {
        cout << "Arquivo filmes.dat nao encontrado. Nada para indexar.\n";
        return;
    }
 
    fseek(f_dados, sizeof(Cabecalho), SEEK_SET);
    Filme f;
    int total = 0;
    while (fread(&f, sizeof(Filme), 1, f_dados) == 1) {
        if (f.titulo[0] != '*') {
            inserir_indice_diretor(f.diretor, f.id_filme);
            inserir_indice_genero(f.genero, f.id_filme);
            total++;
        }
    }
    fclose(f_dados);
 
    cout << "Indices reconstruidos com sucesso a partir de " << total
         << " filme(s) ativo(s) em filmes.dat.\n";
}
 
void exibir_indice_diretor() {
    FILE* f_idx = fopen(ARQ_INDICE_DIRETOR, "rb");
    if (!f_idx) { cout << "Indice de diretor ainda nao existe.\n"; return; }
 
    cout << "\n--- INDICE SECUNDARIO: DIRETOR ---\n";
    IndiceDiretor entrada;
    bool tem_algum = false;
    while (fread(&entrada, sizeof(IndiceDiretor), 1, f_idx) == 1) {
        tem_algum = true;
        vector<int> ids = buscar_indice_diretor(entrada.diretor);
        cout << entrada.diretor << " -> [";
        for (size_t i = 0; i < ids.size(); i++) {
            cout << ids[i];
            if (i + 1 < ids.size()) cout << ", ";
        }
        cout << "]\n";
    }
    if (!tem_algum) cout << "Nenhum diretor indexado ainda.\n";
    fclose(f_idx);
}
 
void exibir_indice_genero() {
    FILE* f_idx = fopen(ARQ_INDICE_GENERO, "rb");
    if (!f_idx) { cout << "Indice de genero ainda nao existe.\n"; return; }
 
    cout << "\n--- INDICE SECUNDARIO: GENERO ---\n";
    IndiceGenero entrada;
    bool tem_algum = false;
    while (fread(&entrada, sizeof(IndiceGenero), 1, f_idx) == 1) {
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
        cin.ignore();
 
        if (opcao == 1) {
            char diretor[50];
            cout << "Diretor: ";
            cin.getline(diretor, 50);
            vector<int> ids = buscar_indice_diretor(diretor);
            if (ids.empty()) {
                cout << "Nenhum filme encontrado para esse diretor.\n";
            } else {
                cout << "IDs encontrados: ";
                for (size_t i = 0; i < ids.size(); i++) cout << ids[i] << " ";
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
                cout << "IDs encontrados: ";
                for (size_t i = 0; i < ids.size(); i++) cout << ids[i] << " ";
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
    } while (opcao != 0);
}
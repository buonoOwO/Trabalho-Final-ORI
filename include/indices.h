#ifndef INDICES_H
#define INDICES_H

#include <vector>
#include "filme.h"

struct IndiceDiretor {
    char diretor[50];
    long primeiro;  
};

struct IndiceGenero {
    char genero[30];
    long primeiro;
};

struct NoLista {
    int  id_filme;  
    long proximo;   
};

struct CabecalhoLista {
    long topo_led; 
};

void inicializar_indices_secundarios();

//Chama apos o CRUD gravar um filme novo
void inserir_indice_diretor(const char* diretor, int id_filme);
void inserir_indice_genero(const char* genero, int id_filme);

//Retornam os ids de filme associados ao valor buscado
std::vector<int> buscar_indice_diretor(const char* diretor);
std::vector<int> buscar_indice_genero(const char* genero);

//Chama apos o CRUD marcar um filme como removido
void remover_indice_diretor(const char* diretor, int id_filme);
void remover_indice_genero(const char* genero, int id_filme);

//Reconstroi os indices do zero varrendo filmes.dat
void reconstruir_indices_secundarios();

void exibir_indice_diretor();
void exibir_indice_genero();

void menu_indices_secundarios();

#endif
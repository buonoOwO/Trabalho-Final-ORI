#include <iostream>
#include "../include/funcoes.h"

using namespace std;

void exibir_menu() {
    cout << "\n=========================================\n";
    cout << "          Catalogo de Filmes\n";
    cout << "=========================================\n";
    cout << "1 - Cadastrar filme\n";
    cout << "2 - Buscar filme por ID\n";
    cout << "3 - Buscar filmes por genero\n";
    cout << "4 - Buscar filmes por diretor\n";
    cout << "5 - Atualizar filme\n";
    cout << "6 - Remover filme\n";
    cout << "7 - Listar todos os filmes\n";
    cout << "8 - Exibir LED\n";
    cout << "9 - Exibir Arvore B\n";
    cout << "10 - Exibir indices secundarios\n";
    cout << "0 - Sair\n";
    cout << "=========================================\n";
    cout << "Escolha uma opcao: ";
}

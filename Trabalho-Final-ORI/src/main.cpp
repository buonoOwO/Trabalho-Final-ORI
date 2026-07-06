#include <iostream>
#include "../include/funcoes.h"

using namespace std;

int main() {

    int opcao;

    do {

        exibir_menu();
        cin >> opcao;

        switch(opcao) {

            case 1:
                cout << "Cadastrar filme...\n";
                break;

            case 2:
                cout << "Buscar por ID...\n";
                break;

            case 3:
                cout << "Buscar por genero...\n";
                break;

            case 4:
                cout << "Buscar por diretor...\n";
                break;

            case 5:
                cout << "Atualizar filme...\n";
                break;

            case 6:
                cout << "Remover filme...\n";
                break;

            case 7:
                cout << "Listar filmes...\n";
                break;

            case 8:
                cout << "Exibir LED...\n";
                break;

            case 9:
                cout << "Exibir Arvore B...\n";
                break;

            case 10:
                cout << "Exibir indices secundarios...\n";
                break;

            case 0:
                cout << "Encerrando o sistema...\n";
                break;

            default:
                cout << "Opcao invalida!\n";
        }

    } while(opcao != 0);

    return 0;
}
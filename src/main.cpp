#include <iostream>
#include <limits>
#include "../include/funcoes.h"
#include "../include/crud.h"
#include "../include/led.h"

using namespace std;

int main() {
    int opcao;

    do {
        exibir_menu();
        
        if (!(cin >> opcao)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Opcao invalida! Tente novamente.\n";
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Limpa o buffer após ler o número

        switch(opcao) {
            case 1:
                cadastrar_filme();
                break;
            case 2:
            case 3:
            case 4:
                buscar_filme();
                break;
            case 5:
                atualizar_filme();
                break;
            case 6:
                remover_filme();
                break;
            case 7:
                listar_filme();
                break;
            case 8:
                menu_exibir_led();
                break;
            case 9:
                cout << "[Arvore B] Funcionalidade a ser integrada futuramente.\n";
                break;
            case 10:
                cout << "[Indices Secundarios] Funcionalidade a ser integrada futuramente.\n";
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

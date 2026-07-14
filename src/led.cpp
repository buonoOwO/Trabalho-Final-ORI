#include <stdio.h>
#include <stdlib.h>
#include "../include/led.h"

// Caminho para o arquivo que gerencia os espaços reutilizáveis
const char* ARQUIVO_LED = "dados/LED.dat";

// Estrutura estática para controle do arquivo de LED.
// Armazena apenas o contador (4 bytes) que indica quantos offsets livres existem atualmente.
struct CabecalhoLED {
    int quantidade;
};

// Inicializa a LED caso o sistema esteja sendo rodado pela primeira vez.
static void inicializar_led_se_nao_existir() {
    FILE* f = fopen(ARQUIVO_LED, "rb");
    if (f == NULL) {
        f = fopen(ARQUIVO_LED, "wb+");
        if (f != NULL) {
            struct CabecalhoLED cab;
            cab.quantidade = 0; // Inicializa a pilha zerada
            fwrite(&cab, sizeof(struct CabecalhoLED), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}

// Operação de DESEMPILHAMENTO (POP)
// Recupera o offset mais recente inserido na pilha (Estratégia LIFO) em complexidade O(1).
long obter_espaco_livre() {
    inicializar_led_se_nao_existir();

    FILE* f_led = fopen(ARQUIVO_LED, "rb+");
    if (f_led == NULL) return -1;

    struct CabecalhoLED cab;
    rewind(f_led);
    fread(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    // Se não há buracos cadastrados, retorna -1 para o CRUD gravar no fim do arquivo (EOF).
    if (cab.quantidade == 0) {
        fclose(f_led);
        return -1;
    }

    long offset_livre;
    
    // Cálculo de Bytes
    // Como a pilha cresce linearmente após o cabeçalho, calculamos a posição do topo (último elemento inserido)
    // pulando o tamanho do cabeçalho de LED (4 bytes) mais o espaço consumido pelos elementos anteriores.
    // Fórmula: Posicao = sizeof(Cabecalho) + (quantidade - 1) * sizeof(long)
    long posicao_topo = sizeof(struct CabecalhoLED) + (cab.quantidade - 1) * sizeof(long);
    
    // Posiciona o ponteiro de arquivo exatamente no topo da pilha para ler o offset livre
    fseek(f_led, posicao_topo, SEEK_SET);
    fread(&offset_livre, sizeof(long), 1, f_led);

    // Decrementa o controle do tamanho da pilha e atualiza o cabeçalho em disco
    cab.quantidade--;
    rewind(f_led);
    fwrite(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    fclose(f_led);
    return offset_livre; // Devolve o offset limpo para o CRUD reescrever
}

// Operação de EMPILHAMENTO (PUSH)
// Registra um novo offset livre no arquivo de LED em complexidade O(1).
void inserir_na_led(long offset) {
    inicializar_led_se_nao_existir();

    FILE* f_led = fopen(ARQUIVO_LED, "rb+");
    if (f_led == NULL) return;

    struct CabecalhoLED cab;
    rewind(f_led);
    fread(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    // Cálculo de bytes
    // Descobre onde escrever o novo offset. A posição de gravação fica logo após 
    // os offsets que já estão registrados na LED.
    // Fórmula: Posicao = sizeof(Cabecalho) + quantidade * sizeof(long)
    long posicao_novo = sizeof(struct CabecalhoLED) + cab.quantidade * sizeof(long);
    
    fseek(f_led, posicao_novo, SEEK_SET);
    fwrite(&offset, sizeof(long), 1, f_led);

    // Incrementa a quantidade de buracos gerenciados e salva o cabeçalho de volta
    cab.quantidade++;
    rewind(f_led);
    fwrite(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    fclose(f_led);
}

// Função administrativa utilizada para demonstrar a integridade da pilha física em disco.
void menu_exibir_led() {
    FILE* f_led = fopen(ARQUIVO_LED, "rb");
    if (f_led == NULL) {
        printf("Nenhum LED.dat encontrado ainda (nenhum filme foi removido).\n");
        return;
    }

    struct CabecalhoLED cab;
    rewind(f_led);
    fread(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    printf("\n--- LISTA DE ESPACOS DISPONIVEIS (LED) ---\n");
    printf("Estrategia de reaproveitamento: LIFO (pilha)\n");
    printf("Quantidade de espacos livres: %d\n", cab.quantidade);

    if (cab.quantidade == 0) {
        fclose(f_led);
        return;
    }

    // Varre sequencialmente o arquivo LED.dat apenas para fins de exibição dos elementos da pilha
    long offset_lido;
    int i;
    for (i = 0; i < cab.quantidade; i++) {
        fread(&offset_lido, sizeof(long), 1, f_led);
        printf("%d) Offset livre no Filmes.dat: %ld", i + 1, offset_lido);
        
        // Evidencia na tela qual é o topo (LIFO) que será o próximo a ser reaproveitado pelo CRUD
        if (i == cab.quantidade - 1) {
            printf("  <-- topo da pilha (proximo a ser reaproveitado)");
        }
        printf("\n");
    }

    fclose(f_led);
}

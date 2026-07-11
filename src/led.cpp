#include <stdio.h>
#include <stdlib.h>
#include "../include/led.h"

const char* ARQUIVO_LED = "dados/LED.dat";

struct CabecalhoLED {
    int quantidade;
};

static void inicializar_led_se_nao_existir() {
    FILE* f = fopen(ARQUIVO_LED, "rb");
    if (f == NULL) {
        f = fopen(ARQUIVO_LED, "wb+");
        if (f != NULL) {
            struct CabecalhoLED cab;
            cab.quantidade = 0;
            fwrite(&cab, sizeof(struct CabecalhoLED), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}

long obter_espaco_livre() {
    inicializar_led_se_nao_existir();

    FILE* f_led = fopen(ARQUIVO_LED, "rb+");
    if (f_led == NULL) return -1;

    struct CabecalhoLED cab;
    rewind(f_led);
    fread(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    if (cab.quantidade == 0) {
        fclose(f_led);
        return -1;
    }

    long offset_livre;
    long posicao_topo = sizeof(struct CabecalhoLED) + (cab.quantidade - 1) * sizeof(long);
    fseek(f_led, posicao_topo, SEEK_SET);
    fread(&offset_livre, sizeof(long), 1, f_led);

    cab.quantidade--;
    rewind(f_led);
    fwrite(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    fclose(f_led);
    return offset_livre;
}

void inserir_na_led(long offset) {
    inicializar_led_se_nao_existir();

    FILE* f_led = fopen(ARQUIVO_LED, "rb+");
    if (f_led == NULL) return;

    struct CabecalhoLED cab;
    rewind(f_led);
    fread(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    long posicao_novo = sizeof(struct CabecalhoLED) + cab.quantidade * sizeof(long);
    fseek(f_led, posicao_novo, SEEK_SET);
    fwrite(&offset, sizeof(long), 1, f_led);

    cab.quantidade++;
    rewind(f_led);
    fwrite(&cab, sizeof(struct CabecalhoLED), 1, f_led);

    fclose(f_led);
}

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

    long offset_lido;
    int i;
    for (i = 0; i < cab.quantidade; i++) {
        fread(&offset_lido, sizeof(long), 1, f_led);
        printf("%d) Offset livre no Filmes.dat: %ld", i + 1, offset_lido);
        if (i == cab.quantidade - 1) {
            printf("  <-- topo da pilha (proximo a ser reaproveitado)");
        }
        printf("\n");
    }

    fclose(f_led);
}
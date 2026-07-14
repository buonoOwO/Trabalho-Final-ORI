#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/arvoreb.h"

// Caminho do arquivo binário que simula a indexação em disco rígido
const char* ARQUIVO_ARVORE = "dados/IndicePrimario.dat";

// Parâmetros de Ordem e Capacidade.
// Como Ordem M = 4 (máximo de 4 filhos por nó interno):
// O número máximo de chaves por nó (MAX_CHAVES) é M - 1 = 3.
// Sob underflow, nós folha devem conter pelo menos ceil(3/2) = 2 chaves.
// Nós internos devem conter pelo menos ceil(4/2) - 1 = 1 chave.
#define ORDEM 4
#define MAX_CHAVES (ORDEM - 1)
#define MIN_CHAVES_FOLHA ((MAX_CHAVES + 1) / 2)
#define MIN_CHAVES_INTERNO (((ORDEM + 1) / 2) - 1)

// Estrutura estática de tamanho fixo para escrita direta em disco.
// Como não há vetores dinâmicos, cada struct 'NoArvoreB' ocupa exatamente o mesmo 
// número de bytes físico na escrita (sizeof(struct NoArvoreB)), essencial para fseek.
struct NoArvoreB {
    int eh_folha;              // Flag indicando se é folha (1) ou nó interno (0)
    int num_chaves;            // Número atual de chaves ativas no nó (max = 3)
    int chaves[MAX_CHAVES];    // Vetor estático de chaves indexadoras (IDs)
    long filhos[ORDEM];        // Ponteiros de arquivo (offsets) para nós filhos (usado se interno)
    long dados[MAX_CHAVES];    // Ponteiros de arquivo (offsets) para o arquivo Filmes.dat (usado se folha)
    long proxima_folha;        // Ponteiro sequencial para a próxima folha à direita (O(1) na listagem)
};

// Cabeçalho com tamanho de 16 bytes que guarda o ponto de entrada 
// do índice (raiz) e o início do encadeamento sequencial das folhas.
struct CabecalhoArvore {
    long offset_raiz;
    long primeira_folha;
};

// Inicializa o arquivo de índices primários caso não exista
static void inicializar_arvore_se_nao_existir() {
    FILE* f = fopen(ARQUIVO_ARVORE, "rb");
    if (f == NULL) {
        f = fopen(ARQUIVO_ARVORE, "wb+");
        if (f != NULL) {
            struct CabecalhoArvore cab;
            cab.offset_raiz = -1;
            cab.primeira_folha = -1;
            fwrite(&cab, sizeof(struct CabecalhoArvore), 1, f);
            fclose(f);
        }
    } else {
        fclose(f);
    }
}

// Cria um nó vazio gravando-o no final do arquivo de índices (EOF)
static long criar_no(FILE* f, int eh_folha) {
    fseek(f, 0, SEEK_END);
    long offset = ftell(f); // Pega a posição física onde o nó será escrito

    struct NoArvoreB no;
    memset(&no, 0, sizeof(struct NoArvoreB));
    no.eh_folha = eh_folha;
    no.num_chaves = 0;
    no.proxima_folha = -1;

    int i;
    for (i = 0; i < ORDEM; i++) no.filhos[i] = -1;

    fwrite(&no, sizeof(struct NoArvoreB), 1, f);
    return offset; // Retorna o ponteiro de disco (offset) para quem chamou
}

// Lê de forma direta e cirúrgica um nó do arquivo usando seu offset físico
static void ler_no(FILE* f, long offset, struct NoArvoreB* no) {
    fseek(f, offset, SEEK_SET);
    fread(no, sizeof(struct NoArvoreB), 1, f);
}

// Grava as alterações de um nó modificado de volta para o seu local de origem
static void escrever_no(FILE* f, long offset, struct NoArvoreB* no) {
    fseek(f, offset, SEEK_SET);
    fwrite(no, sizeof(struct NoArvoreB), 1, f);
}


// Busca indexada (TEMPO O(log n))
long buscar_offset_dado(int id_filme) {
    inicializar_arvore_se_nao_existir();
    FILE* f = fopen(ARQUIVO_ARVORE, "rb");
    if (f == NULL) return -1;

    struct CabecalhoArvore cab;
    rewind(f);
    fread(&cab, sizeof(struct CabecalhoArvore), 1, f);

    if (cab.offset_raiz == -1) {
        fclose(f);
        return -1;
    }

    struct NoArvoreB no;
    long atual = cab.offset_raiz;

    // Navegação descendente.
    // O loop percorre os nós internos direcionando a busca pelo filho correto,
    // parando apenas quando alcança uma folha (onde os ponteiros reais estão).
    while (1) {
        ler_no(f, atual, &no);
        if (no.eh_folha) break; // Em árvores B+, dados residem apenas nas folhas!
        
        int i = 0;
        // Varre chaves internas para escolher em qual caminho descer
        while (i < no.num_chaves && id_filme >= no.chaves[i]) i++;
        atual = no.filhos[i]; // Desce para o offset do nó filho
    }

    // Busca exata pela chave de ID dentro do nó folha alcançado
    int i;
    for (i = 0; i < no.num_chaves; i++) {
        if (no.chaves[i] == id_filme) {
            fclose(f);
            return no.dados[i]; // Retorna o offset físico para o arquivo de filmes.dat
        }
    }

    fclose(f);
    return -1;
}

// inserção e divisão (SPLIT)

// Realiza a inserção ordenada no nó folha e trata o Split caso exceda MAX_CHAVES
static int inserir_na_folha(FILE* f, long offset_no, int id_filme, long offset_dado,
                             int* chave_promovida, long* offset_novo_no) {
    struct NoArvoreB no;
    ler_no(f, offset_no, &no);

    int chaves_temp[MAX_CHAVES + 1];
    long dados_temp[MAX_CHAVES + 1];

    // Encontra a posição correta de inserção mantendo a ordenação
    int pos = 0;
    while (pos < no.num_chaves && no.chaves[pos] < id_filme) pos++;

    // Cria cópia temporária maior para realizar a inserção antes da divisão
    int i;
    for (i = 0; i < pos; i++) {
        chaves_temp[i] = no.chaves[i];
        dados_temp[i] = no.dados[i];
    }
    chaves_temp[pos] = id_filme;
    dados_temp[pos] = offset_dado;
    for (i = pos; i < no.num_chaves; i++) {
        chaves_temp[i + 1] = no.chaves[i];
        dados_temp[i + 1] = no.dados[i];
    }

    int total = no.num_chaves + 1;

    // Se o nó não está cheio, escreve diretamente e retorna sem split (0)
    if (total <= MAX_CHAVES) {
        no.num_chaves = total;
        for (i = 0; i < total; i++) {
            no.chaves[i] = chaves_temp[i];
            no.dados[i] = dados_temp[i];
        }
        escrever_no(f, offset_no, &no);
        return 0;
    }

    // Logica do split em folha
    // Se estourou, dividimos ao meio. Como é Árvore B+:
    // 1. A metade esquerda fica no nó original.
    // 2. Criamos um novo nó à direita para alocar a metade direita.
    // 3. A chave mais baixa do nó da direita é copiada/promovida para o pai.
    int meio = total / 2;

    struct NoArvoreB esquerda;
    memcpy(&esquerda, &no, sizeof(struct NoArvoreB));
    esquerda.num_chaves = meio;
    for (i = 0; i < meio; i++) {
        esquerda.chaves[i] = chaves_temp[i];
        esquerda.dados[i] = dados_temp[i];
    }

    long offset_direita = criar_no(f, 1); // Cria nova folha em disco
    struct NoArvoreB direita;
    ler_no(f, offset_direita, &direita);
    direita.num_chaves = total - meio;
    for (i = meio; i < total; i++) {
        direita.chaves[i - meio] = chaves_temp[i];
        direita.dados[i - meio] = dados_temp[i];
    }

    // encadeamento sequencial
    // Preserva a lista encadeada das folhas atualizando os ponteiros 'proxima_folha'.
    // Garante que possamos varrer todas as chaves sequencialmente sem subir na árvore.
    direita.proxima_folha = esquerda.proxima_folha;
    esquerda.proxima_folha = offset_direita;

    escrever_no(f, offset_no, &esquerda);
    escrever_no(f, offset_direita, &direita);

    *chave_promovida = direita.chaves[0]; // Cópia da chave promovida
    *offset_novo_no = offset_direita;
    return 1; // Retorna 1 indicando que houve split
}

// Insere a chave promovida de uma folha/interno dentro de um nó interno
static int inserir_no_interno(FILE* f, long offset_no, int chave_promovida, long offset_filho_direito,
                               int* chave_promovida_acima, long* offset_novo_no_acima) {
    struct NoArvoreB no;
    ler_no(f, offset_no, &no);

    int pos = 0;
    while (pos < no.num_chaves && chave_promovida > no.chaves[pos]) pos++;

    int chaves_temp[MAX_CHAVES + 1];
    long filhos_temp[ORDEM + 1];

    int i;
    for (i = 0; i < pos; i++) chaves_temp[i] = no.chaves[i];
    chaves_temp[pos] = chave_promovida;
    for (i = pos; i < no.num_chaves; i++) chaves_temp[i + 1] = no.chaves[i];

    for (i = 0; i <= pos; i++) filhos_temp[i] = no.filhos[i];
    filhos_temp[pos + 1] = offset_filho_direito;
    for (i = pos + 1; i < no.num_chaves + 1; i++) filhos_temp[i + 1] = no.filhos[i];

    int total = no.num_chaves + 1;

    // Se o nó interno não está cheio, escreve e retorna
    if (total <= MAX_CHAVES) {
        no.num_chaves = total;
        for (i = 0; i < total; i++) no.chaves[i] = chaves_temp[i];
        for (i = 0; i <= total; i++) no.filhos[i] = filhos_temp[i];
        escrever_no(f, offset_no, &no);
        return 0;
    }

    // Lógica do split em nó interno
    // Ao contrário do nó folha, em nós internos a chave do meio é "REMOVIDA" (puxada)
    // para cima, não sendo copiada nem repetida na esquerda ou na direita.
    int meio = total / 2;

    struct NoArvoreB esquerda;
    memcpy(&esquerda, &no, sizeof(struct NoArvoreB));
    esquerda.num_chaves = meio;
    for (i = 0; i < meio; i++) esquerda.chaves[i] = chaves_temp[i];
    for (i = 0; i <= meio; i++) esquerda.filhos[i] = filhos_temp[i];

    long offset_direita = criar_no(f, 0); // Cria novo nó interno
    struct NoArvoreB direita;
    ler_no(f, offset_direita, &direita);
    direita.num_chaves = total - meio - 1;
    for (i = meio + 1; i < total; i++) direita.chaves[i - meio - 1] = chaves_temp[i];
    for (i = meio + 1; i <= total; i++) direita.filhos[i - meio - 1] = filhos_temp[i];

    escrever_no(f, offset_no, &esquerda);
    escrever_no(f, offset_direita, &direita);

    *chave_promovida_acima = chaves_temp[meio]; // Chave do meio sobe de vez para o pai
    *offset_novo_no_acima = offset_direita;
    return 1;
}

// Navegação recursiva descendente para a inserção
static int inserir_recursivo(FILE* f, long offset_no, int id_filme, long offset_dado,
                              int* chave_promovida, long* offset_novo_no) {
    struct NoArvoreB no;
    ler_no(f, offset_no, &no);

    if (no.eh_folha) {
        return inserir_na_folha(f, offset_no, id_filme, offset_dado, chave_promovida, offset_novo_no);
    }

    int i = 0;
    while (i < no.num_chaves && id_filme >= no.chaves[i]) i++;

    int chave_prom_filho;
    long offset_novo_filho;
    int houve_split = inserir_recursivo(f, no.filhos[i], id_filme, offset_dado,
                                         &chave_prom_filho, &offset_novo_filho);

    if (!houve_split) return 0;

    // Se o filho dividiu, insere o elemento promovido no nível atual
    return inserir_no_interno(f, offset_no, chave_prom_filho, offset_novo_filho,
                               chave_promovida, offset_novo_no);
}

// Função pública para disparar o processo de inserção e indexação
void inserir_arvore(int id_filme, long offset_dado) {
    inicializar_arvore_se_nao_existir();
    FILE* f = fopen(ARQUIVO_ARVORE, "rb+");
    if (f == NULL) return;

    struct CabecalhoArvore cab;
    rewind(f);
    fread(&cab, sizeof(struct CabecalhoArvore), 1, f);

    // Caso a árvore esteja vazia, cria a primeira folha que também será a raiz
    if (cab.offset_raiz == -1) {
        long offset_folha = criar_no(f, 1);
        struct NoArvoreB folha;
        ler_no(f, offset_folha, &folha);
        folha.num_chaves = 1;
        folha.chaves[0] = id_filme;
        folha.dados[0] = offset_dado;
        escrever_no(f, offset_folha, &folha);

        cab.offset_raiz = offset_folha;
        cab.primeira_folha = offset_folha;
        rewind(f);
        fwrite(&cab, sizeof(struct CabecalhoArvore), 1, f);
        fclose(f);
        return;
    }

    int chave_promovida;
    long offset_novo_no;
    int houve_split = inserir_recursivo(f, cab.offset_raiz, id_filme, offset_dado,
                                         &chave_promovida, &offset_novo_no);

    // Crescimento da árvore
    // Se a raiz sofrer divisão (split), a árvore cresce 1 nível para cima.
    // Criamos uma nova raiz apontando para a raiz antiga e o novo irmão gerado.
    if (houve_split) {
        long nova_raiz = criar_no(f, 0);
        struct NoArvoreB raiz;
        ler_no(f, nova_raiz, &raiz);
        raiz.num_chaves = 1;
        raiz.chaves[0] = chave_promovida;
        raiz.filhos[0] = cab.offset_raiz;
        raiz.filhos[1] = offset_novo_no;
        escrever_no(f, nova_raiz, &raiz);

        cab.offset_raiz = nova_raiz;
        rewind(f);
        fwrite(&cab, sizeof(struct CabecalhoArvore), 1, f);
    }

    fclose(f);
}

// remoção e balanceamento (underflow

// Remove a chave diretamente de um nó folha
static int remover_de_folha(FILE* f, long offset_no, int id_filme, int* achou) {
    struct NoArvoreB no;
    ler_no(f, offset_no, &no);

    int pos = -1;
    int i;
    for (i = 0; i < no.num_chaves; i++) {
        if (no.chaves[i] == id_filme) { pos = i; break; }
    }

    if (pos == -1) {
        *achou = 0;
        return 0;
    }

    // Rotaciona chaves restantes para manter a ordenação sequencial compacta
    for (i = pos; i < no.num_chaves - 1; i++) {
        no.chaves[i] = no.chaves[i + 1];
        no.dados[i] = no.dados[i + 1];
    }
    no.num_chaves--;
    escrever_no(f, offset_no, &no);

    *achou = 1;
    // Retorna se o nó violou a quantidade mínima exigida (underflow)
    return no.num_chaves < MIN_CHAVES_FOLHA;
}

// Tratamento do underflow
// Se um nó cai abaixo do limite de chaves exigido pelo rebalanceamento da Árvore:
// 1. Tenta pegar uma chave emprestada do irmão da esquerda (Redistribuição).
// 2. Tenta pegar emprestada do irmão da direita (Redistribuição).
// 3. Se ambos irmãos tiverem apenas o mínimo, funde os dois nós (Fusão / Merge).
static void corrigir_underflow(FILE* f, long offset_pai, int idx_filho) {
    struct NoArvoreB pai;
    ler_no(f, offset_pai, &pai);

    long offset_filho = pai.filhos[idx_filho];
    struct NoArvoreB filho;
    ler_no(f, offset_filho, &filho);

    int eh_folha = filho.eh_folha;
    int min_chaves = eh_folha ? MIN_CHAVES_FOLHA : MIN_CHAVES_INTERNO;
    int i;

    // CASO 1: Empréstimo do irmão esquerdo (Redistribuição à direita)
    if (idx_filho > 0) {
        long offset_esq = pai.filhos[idx_filho - 1];
        struct NoArvoreB esq;
        ler_no(f, offset_esq, &esq);

        if (esq.num_chaves > min_chaves) {
            if (eh_folha) {
                for (i = filho.num_chaves; i > 0; i--) {
                    filho.chaves[i] = filho.chaves[i - 1];
                    filho.dados[i] = filho.dados[i - 1];
                }
                filho.chaves[0] = esq.chaves[esq.num_chaves - 1];
                filho.dados[0] = esq.dados[esq.num_chaves - 1];
                filho.num_chaves++;
                esq.num_chaves--;
                pai.chaves[idx_filho - 1] = filho.chaves[0];
            } else {
                for (i = filho.num_chaves; i > 0; i--) filho.chaves[i] = filho.chaves[i - 1];
                for (i = filho.num_chaves + 1; i > 0; i--) filho.filhos[i] = filho.filhos[i - 1];
                filho.chaves[0] = pai.chaves[idx_filho - 1];
                filho.filhos[0] = esq.filhos[esq.num_chaves];
                filho.num_chaves++;
                pai.chaves[idx_filho - 1] = esq.chaves[esq.num_chaves - 1];
                esq.num_chaves--;
            }
            escrever_no(f, offset_esq, &esq);
            escrever_no(f, offset_filho, &filho);
            escrever_no(f, offset_pai, &pai);
            return;
        }
    }

    // CASO 2: Empréstimo do irmão direito (Redistribuição à esquerda)
    if (idx_filho < pai.num_chaves) {
        long offset_dir = pai.filhos[idx_filho + 1];
        struct NoArvoreB dir;
        ler_no(f, offset_dir, &dir);

        if (dir.num_chaves > min_chaves) {
            if (eh_folha) {
                filho.chaves[filho.num_chaves] = dir.chaves[0];
                filho.dados[filho.num_chaves] = dir.dados[0];
                filho.num_chaves++;
                for (i = 0; i < dir.num_chaves - 1; i++) {
                    dir.chaves[i] = dir.chaves[i + 1];
                    dir.dados[i] = dir.dados[i + 1];
                }
                dir.num_chaves--;
                pai.chaves[idx_filho] = dir.chaves[0];
            } else {
                int nova_separadora = dir.chaves[0];
                filho.chaves[filho.num_chaves] = pai.chaves[idx_filho];
                filho.filhos[filho.num_chaves + 1] = dir.filhos[0];
                filho.num_chaves++;
                pai.chaves[idx_filho] = nova_separadora;

                for (i = 0; i < dir.num_chaves - 1; i++) dir.chaves[i] = dir.chaves[i + 1];
                for (i = 0; i < dir.num_chaves; i++) dir.filhos[i] = dir.filhos[i + 1];
                dir.num_chaves--;
            }
            escrever_no(f, offset_dir, &dir);
            escrever_no(f, offset_filho, &filho);
            escrever_no(f, offset_pai, &pai);
            return;
        }
    }

    // CASO 3: FUSÃO (MERGE) — Nenhum vizinho pode doar chaves. Juntamos dois irmãos.
    if (idx_filho > 0) {
        // Funde com o irmão esquerdo
        long offset_esq = pai.filhos[idx_filho - 1];
        struct NoArvoreB esq;
        ler_no(f, offset_esq, &esq);

        if (eh_folha) {
            for (i = 0; i < filho.num_chaves; i++) {
                esq.chaves[esq.num_chaves + i] = filho.chaves[i];
                esq.dados[esq.num_chaves + i] = filho.dados[i];
            }
            esq.num_chaves += filho.num_chaves;
            esq.proxima_folha = filho.proxima_folha; // Corrige o encadeamento das folhas
        } else {
            esq.chaves[esq.num_chaves] = pai.chaves[idx_filho - 1];
            for (i = 0; i < filho.num_chaves; i++) esq.chaves[esq.num_chaves + 1 + i] = filho.chaves[i];
            for (i = 0; i <= filho.num_chaves; i++) esq.filhos[esq.num_chaves + 1 + i] = filho.filhos[i];
            esq.num_chaves += filho.num_chaves + 1;
        }
        escrever_no(f, offset_esq, &esq);

        // Corrige o pai reduzindo o tamanho do vetor do pai
        for (i = idx_filho - 1; i < pai.num_chaves - 1; i++) pai.chaves[i] = pai.chaves[i + 1];
        for (i = idx_filho; i < pai.num_chaves; i++) pai.filhos[i] = pai.filhos[i + 1];
        pai.num_chaves--;
        escrever_no(f, offset_pai, &pai);
    } else {
        // Funde com o irmão direito
        long offset_dir = pai.filhos[idx_filho + 1];
        struct NoArvoreB dir;
        ler_no(f, offset_dir, &dir);

        if (eh_folha) {
            for (i = 0; i < dir.num_chaves; i++) {
                filho.chaves[filho.num_chaves + i] = dir.chaves[i];
                filho.dados[filho.num_chaves + i] = dir.dados[i];
            }
            filho.num_chaves += dir.num_chaves;
            filho.proxima_folha = dir.proxima_folha;
        } else {
            filho.chaves[filho.num_chaves] = pai.chaves[idx_filho];
            for (i = 0; i < dir.num_chaves; i++) filho.chaves[filho.num_chaves + 1 + i] = dir.chaves[i];
            for (i = 0; i <= dir.num_chaves; i++) filho.filhos[filho.num_chaves + 1 + i] = dir.filhos[i];
            filho.num_chaves += dir.num_chaves + 1;
        }
        escrever_no(f, offset_filho, &filho);

        for (i = idx_filho; i < pai.num_chaves - 1; i++) pai.chaves[i] = pai.chaves[i + 1];
        for (i = idx_filho + 1; i < pai.num_chaves; i++) pai.filhos[i] = pai.filhos[i + 1];
        pai.num_chaves--;
        escrever_no(f, offset_pai, &pai);
    }
}

// Navegação descendente recursiva buscando remover a chave
static int remover_recursivo(FILE* f, long offset_no, int id_filme, int* achou) {
    struct NoArvoreB no;
    ler_no(f, offset_no, &no);

    if (no.eh_folha) {
        return remover_de_folha(f, offset_no, id_filme, achou);
    }

    int i = 0;
    while (i < no.num_chaves && id_filme >= no.chaves[i]) i++;

    int underflow_filho = remover_recursivo(f, no.filhos[i], id_filme, achou);

    if (!(*achou)) return 0;

    // Se o filho teve underflow, aplica as correções no nível atual
    if (underflow_filho) {
        corrigir_underflow(f, offset_no, i);
        ler_no(f, offset_no, &no);
        return no.num_chaves < MIN_CHAVES_INTERNO;
    }

    return 0;
}

// Função pública que inicia a remoção do ID do índice da Árvore B+
void remover_arvore(int id_filme) {
    inicializar_arvore_se_nao_existir();
    FILE* f = fopen(ARQUIVO_ARVORE, "rb+");
    if (f == NULL) return;

    struct CabecalhoArvore cab;
    rewind(f);
    fread(&cab, sizeof(struct CabecalhoArvore), 1, f);

    if (cab.offset_raiz == -1) {
        fclose(f);
        return;
    }

    int achou = 0;
    remover_recursivo(f, cab.offset_raiz, id_filme, &achou);

    if (!achou) {
        fclose(f);
        return;
    }

    struct NoArvoreB raiz;
    ler_no(f, cab.offset_raiz, &raiz);

    // DEFESA ORAL (REDUÇÃO DA ALTURA DA ÁRVORE):
    // Se a raiz perdeu todas as chaves devido à fusão descendente, 
    // seu único filho restante (filhos[0]) assume a posição de nova raiz global.
    if (!raiz.eh_folha && raiz.num_chaves == 0) {
        cab.offset_raiz = raiz.filhos[0];
        rewind(f);
        fwrite(&cab, sizeof(struct CabecalhoArvore), 1, f);
    } else if (raiz.eh_folha && raiz.num_chaves == 0) {
        // Árvore tornou-se completamente vazia
        cab.offset_raiz = -1;
        cab.primeira_folha = -1;
        rewind(f);
        fwrite(&cab, sizeof(struct CabecalhoArvore), 1, f);
    }

    fclose(f);
}

// visualização e integridade

// Função do menu administrativo para listagem sequencial rápida de todas as chaves
void menu_exibir_arvore() {
    inicializar_arvore_se_nao_existir();
    FILE* f = fopen(ARQUIVO_ARVORE, "rb");
    if (f == NULL) {
        printf("Nenhum indice primario encontrado.\n");
        return;
    }

    struct CabecalhoArvore cab;
    rewind(f);
    fread(&cab, sizeof(struct CabecalhoArvore), 1, f);

    printf("\n--- INDICE PRIMARIO (ARVORE B+) ---\n");

    if (cab.primeira_folha == -1) {
        printf("Arvore vazia.\n");
        fclose(f);
        return;
    }

    long atual = cab.primeira_folha;
    struct NoArvoreB folha;
    int i;

    // Listagem em tempo O(n)
    // Como é uma Árvore B+, não precisamos fazer percurso em-ordem (in-order traversal)
    // recursivo subindo e descendo nós para ler em ordem crescente.
    // Basta seguir o encadeamento físico direto de folha em folha pelo campo 'proxima_folha'.
    while (atual != -1) {
        // seggurança: Previne leitura indevida do cabeçalho de metadados como se fosse nó
        if (atual < (long)sizeof(struct CabecalhoArvore)) {
            printf("[Erro] Ponteiro corrompido detectado na estrutura da arvore (%ld).\n", atual);
            break;
        }

        ler_no(f, atual, &folha);

        // segurança: Previne loops infinitos ou Falha de Segmentação em caso de corrupção
        if (folha.num_chaves < 0 || folha.num_chaves > MAX_CHAVES) {
            printf("[Erro] No corrompido com numero de chaves invalido (%d).\n", folha.num_chaves);
            break;
        }

        for (i = 0; i < folha.num_chaves; i++) {
            printf("ID: %d -> Offset no Filmes.dat: %ld\n", folha.chaves[i], folha.dados[i]);
        }
        
        atual = folha.proxima_folha; // Pula para a próxima folha à direita física no disco
    }

    fclose(f);
}

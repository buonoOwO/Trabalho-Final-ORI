#ifndef FILME_H // para impedir que o (.h) seja incluído mais de uma vez na compilação
#define FILME_H

struct Cabecalho {
    int proximo_id;       // Guarda o próximo ID disponível (1, 2, 3...)
    long offset_led;      // Topo da Lista de Espaços Disponíveis
};

struct Filme {
    int id_filme;              // Chave primária
    char titulo[100];
    char diretor[50];          // Chave secundária
    char genero[30];           // Chave secundária
    int ano;
    int duracao;               // Em minutos
    float nota;
    char data_assistido[11];   // Formato DD/MM/AAAA
    bool favorito;
    char opiniao[300];         // comentário do usuário
};

#endif

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -O2 -Iinclude
LDFLAGS  :=
 
SRC_DIR  := src
OBJ_DIR  := obj
DADOS_DIR:= dados
 
TARGET   := catalogo
 
# Pega todos os .cpp de src/ automaticamente (nao precisa listar na mao)
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
 
# Regra padrao
all: $(TARGET)
 
# Link final: junta todos os .o num unico executavel
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build concluido -> ./$(TARGET)"
 
# Compila cada .cpp em um .o, gerando tambem o .d de dependencias
# (-MMD -MP faz o g++ registrar de quais .h cada .cpp depende,
#  assim, se voce editar um .h, o make recompila so quem usa ele)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
 
# Garante que as pastas existam antes de compilar/rodar
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
 
$(DADOS_DIR):
	mkdir -p $(DADOS_DIR)
 
# Inclui os arquivos de dependencia gerados (se existirem)
-include $(DEPS)
 
# Compila e ja roda o programa
run: $(TARGET) | $(DADOS_DIR)
	./$(TARGET)
 
# Recompila do zero
rebuild: clean all
 
# Remove binarios e objetos (mantem os .dat em dados/)
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
 
# Remove tudo, incluindo os arquivos de dados persistidos
clean-dados:
	rm -f $(DADOS_DIR)/*.dat
 
.PHONY: all run rebuild clean clean-dados
Trabalho do Grau A
Editor de objetos com luz e animação bezier

Feito por Vitor Hugo Silva

para compilar é necessario msys2, glm, opengl, biblioteca stb_image (já inclusa), json nlohmann (já incluso), glad gerada e cmake, alem de uma variavel de path para o visual code.

Exemplo de uso:

O programa já vem com uma situação simulada de uso, um json de cena pronta, pode ser modificado sem compilar para rodar o programa, para compilar com cmake, primeiro digite ctrl + shift + p para abrir a aba de comando no vscode, selecione scan for kits e depois select kit com o compilador gcc, apos isso use o comando build para o cmake criar uma pasta com o executavel, e então entre na pasta build pelo terminal no vscode usando o comando cd build e execute o compilado.

Instruções:

WASD move a camera, TAB muda o cubo selecionado, tambem é possivel usar o botão direito do mouse para selecionar o objeto agora, R modo de rotação usando as teclas X,Y,Z para rotacionar, T modo de translatação usando as teclas I,K,J,L,U,O para translatar a posição do selecionado, E para escalar usando UP e DOWN para aumentar ou diminuir a escala, P para perspectiva ortografica ou camera, e M para modo wireframe, 1 e 2 movem a luz.

Detalhes tecnicos:

STB_IMAGE usado para carregar arquivos de imagens e enviar para o processamento como textura, JSON é responsavel por utilizar a biblioteca nlohmann que permite configurar uma cena de maneira intuitiva e sem necessidade de compilar, tornando ela menos hardcoded.

A maioria das instruções do programa como o LoaderObj e callbacks são executadas pela CPU, enquanto a GPU é encarregada de criar e transformar os vertices, criar os shaders e iluminação e assim dando output dos pixels na tela, existe algumas tarefas no qual a CPU trabalha com a GPU, sendo elas, arrays de vertices da CPU para a memoria da GPU, pointes para dizer a GPU como ler dados do vertex e tambem instruções da CPU para a GPU de quando executar outras funções como desenhar os vertices.

MeshData e Objeto são structs para armazenar dados de vertices, assim servindo como uma bandeja de dados para carregar na GPU, e o Objeto serve para representar objetos 3D com VAO com propriedades.

de shaders temos vertex e fragment, o vertex é um shader de vertice para calcular gl_Position que é uma projeção, já o fragment é para a iluminação Phong, com a diferenciação entre wireframe e normal.

LoaderOBJ para abrir arquivos do diretorio com nome obj"n", cria arrays de posição e normal e retorna MeshData com array e contagem com os valores v de vertices, vn de normais e f para faces, sendo faces dividias em v/vt/vn.

Antes do Main cria um shader no qual define a fonte de vertex e fragment e anexa em program, e compila.

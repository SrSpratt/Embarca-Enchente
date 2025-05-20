# Embarcatech-Residência
## Embarca-Enchentes
#### Autor:
* Roberto Vítor Lima Gomes Rodrigues

### Enchentes
Dando continuidade à utilização de multitarefas, foi construído um sistema de alerta de enchentes utilizando o Joystick, o display, a matriz de LEDs, os LEDs RGB e os Buzzers.

#### Vídeo de funcionamento
* 


#### Instruções de compilação
Certifique-se de que você tem o ambiente configurado conforme abaixo:
* Pico SDK instalado e configurado.
* VSCode com todas as extensões configuradas, CMake e Make instalados.
* Clone o repositório e abra a pasta do projeto, a extensão Pi Pico criará a pasta build
* Clique em Compile na barra inferior, do lado direito (ao lado esquerdo de RUN | PICO SDK)
* Verifique se gerou o arquivo .uf2
* Conecte a placa BitDogLab e ponha-a em modo BOOTSEL
* Arraste o arquivo até a placa, que o programa se iniciará

#### Manual do programa
Ao executar o programa, o sistema se iniciará:
* A matriz de LEDs mostrará um quadrado verde
    * O display mostrará o nível de cheia das águas e o porcentagem de chuvas
    * O buzzer repetirá um toque fraco a cada 500ms
    * O LED RGB ficará mais verde ou mais azul de acordo com a movimentação do joystick no eixo horizontal ou no vertical, respectivamente:
    * Ao mexer no joystick na horizontal, a porcentagem de nível de água aumentará à medida que o movimento se distanciar do centro
    * Ao mexer no joystick na vertical, a porcentagem de chuva aumentará à medida que o movimento se distanciar do centro
* Ao chegar a cerca de 45%~50% em qualquer eixo, o quadrado da matriz de LEDs ficará amarelo, e o toque do buzzer ficará mais rápido, irá para 200ms
* Ao chegar a cerca de 70% no eixo horizontal, ou 80% no eixo horizontal, o LED vermelho se acenderá, o display mostrará o aviso de perigo, e a matriz mostrará uma exclamação vermelha
* No nível de perigo, o buzzer aumentará a intensidade sonorá e também a frequência de toque, tocando a intervalos de 100ms

---
# Idioma ptBR
# Projeto ESP32-CAM com WebAssembly (WAMR)

Este projeto tem como objetivo configurar e executar um firmware customizado na placa **ESP32-CAM**, integrando a captura de imagem com a execução de módulos WebAssembly através do **WASM-Micro-Runtime (WAMR)**.

## 1. Instalação e Configuração do Ambiente ESP32-CAM

### 📦 Instalar o ESP-IDF

- Faça o download do ESP-IDF no endereço oficial:  
  [https://dl.espressif.com/dl/esp-idf/](https://dl.espressif.com/dl/esp-idf/)

- Adicione o executável `idf.py.exe` ao **PATH** do sistema:  
C:\Espressif\tools\idf-exe\1.0.3


### 📁 Criar o Projeto

- Use o template oficial como base do projeto:  
[Sample Project - ESP-IDF GitHub](https://github.com/espressif/esp-idf/tree/master/tools/templates/sample_project)

- Execute o primeiro build para gerar automaticamente o arquivo `sdkconfig`:
```bash
idf.py build
```

### 🧠 Habilitar PSRAM (memória externa)
* Ative os 4MB de PSRAM manualmente no `sdkconfig`:
```
CONFIG_SPIRAM=y
```

### 🧠 Habilitar na placa ESP32-CAM o PSRAM:

* Execute `idf.py menuconfig` e habilite no segunte menu: `Component config → ESP PSRAM | [*] Support for external | SPI-connected RAM`


### 🗂️ Configurar Partição Customizada

* No `menuconfig`, vá para:

  ```
  Partition Table → Partition Table → Custom partition table CSV
  ```

* Insira o seguinte conteúdo no arquivo `partitions.csv` na raiz do projeto:

  ```csv
  # Name, Type, SubType, Offset, Size, Flags
  nvs,data,nvs,0x9000,24K,
  phy_init,data,phy,0xf000,4K,
  factory,app,factory,0x10000,2M,
  spiffs,data,spiffs,0x210000,0x100000,
  ```

  Isso reserva 2MB para o firmware na partição `factory` e 1MB para arquivos na partição `spiffs`.

---

## 2. Adicionar WebAssembly Micro Runtime (WAMR)

* No arquivo `idf_component.yml`, adicione:

  ```yaml
  ## IDF Component Manager Manifest File
  dependencies:
    wasm-micro-runtime:
      version: "^1"
    idf:
      version: ">=4.4"
    espressif/esp32-camera:
      version: "*"
  ```

> O gerenciador de componentes do ESP-IDF buscará automaticamente as dependências durante o build. Não é necessário clonar manualmente repositórios como o WAMR.

---

## 3. Compilar, Gravar e Monitorar o Firmware

Utilize um terminal do ESP-IDF como **ESP-IDF PowerShell** ou **ESP-IDF CMD** e execute:

```bash
idf.py set-target esp32 # utilizado somente 1 vez para setar o ambiente
idf.py fullclean
idf.py build
idf.py flash monitor
```

* O comando `monitor` permite visualizar a saída de logs e mensagens da ESP32-CAM diretamente no seu terminal.

---

## ✅ Requisitos

* Placa: **ESP32-CAM com suporte a PSRAM**
* Sistema: **Windows (recomendado)** com ESP-IDF instalado
* Ferramentas:

  * Git
  * Python 3.8+
  * `idf.py` configurado no PATH

---
* Comandos `wat2wasm` e `xxd`
O comando `wat2wasm` esta dentro do programa WABT que precisa ser feito o download no seguinte endereço [WABT GitHub](https://github.com/WebAssembly/wabt/releases)

O comando `xxd` precisa ser feito o download no seguinte endereço [xxd](https://sourceforge.net/projects/xxd-for-windows/)

Após o download de ambos os programas, inseri-los na raiz do Windows e nas variaveis de ambiente.

O comando `wat2wasm` serve para converter o arquivo wat para o binario wasm
```sh
wat2wasm .\hello_word.wat -o .\hello_word.wasm
xxd -i ola_mundo.wasm > test_wasm.h
```

O comando `xxd` serve para converter o arquivo wasm para um array binario que será carragado e lido pelo código C.
```sh
xxd -i ola_mundo.wasm > test_wasm.h
```

---
* Variaveis de Ambiente: 
```sh
IDF-PATH: C:\Espressif\frameworks\esp-idf-v5.3.1\
PATH: 
    C:\Espressif\tools\idf-exe\1.0.3\ 
    C:\xxd 
    C:\Program Files (x86)\WABT\bin
```

## 📎 Observação

O projeto assume que o firmware será executado com suporte a WebAssembly via WAMR. Certifique-se de que o seu código WebAssembly esteja preparado para lidar com buffers de imagem capturados pela câmera integrada.
# Projeto ESP32-CAM com WebAssembly (WAMR) | ESP32-CAM Project with WebAssembly (WAMR)

Este projeto tem como objetivo configurar e executar um firmware customizado na placa **ESP32-CAM**, integrando a captura de imagem com a execução de módulos WebAssembly através do **WASM-Micro-Runtime (WAMR)**.  
This project aims to configure and run a custom firmware on the **ESP32-CAM** board, integrating image capture with the execution of WebAssembly modules through the **WASM-Micro-Runtime (WAMR)**.

---

## 1. Instalação e Configuração do Ambiente ESP32-CAM  
## 1. Installing and Setting Up the ESP32-CAM Environment

### 📦 Instalar o ESP-IDF | Install ESP-IDF

- Faça o download do ESP-IDF:  
  Download ESP-IDF from:  
  [https://dl.espressif.com/dl/esp-idf/](https://dl.espressif.com/dl/esp-idf/)

- Adicione `idf.py.exe` ao **PATH**:  
  Add `idf.py.exe` to the **PATH** environment variable:  
```

C:\Espressif\tools\idf-exe\1.0.3

````

### 📁 Criar o Projeto | Create the Project

- Use o template oficial:  
Use the official template as a base project:  
[Sample Project - ESP-IDF GitHub](https://github.com/espressif/esp-idf/tree/master/tools/templates/sample_project)

- Execute o primeiro build:  
Run the initial build to generate the `sdkconfig`:
```bash
idf.py build
````

### 🧠 Habilitar PSRAM | Enable PSRAM

* Ative manualmente no `sdkconfig`:
  Manually activate in `sdkconfig`:

  ```
  CONFIG_SPIRAM=y
  ```

* Ou via `menuconfig`:
  Or via `menuconfig`:

  ```bash
  idf.py menuconfig
  ```

  ```
  Component config → ESP PSRAM → [*] Support for external SPI-connected RAM
  ```

### 🗂️ Partição Customizada | Custom Partition Table

* Vá em `menuconfig`:
  Go to `menuconfig`:

  ```
  Partition Table → Partition Table → Custom partition table CSV
  ```

* Insira o arquivo `partitions.csv`:
  Insert the following into `partitions.csv` at the root of the project:

  ```csv
  # Name, Type, SubType, Offset, Size, Flags
  nvs,data,nvs,0x9000,24K,
  phy_init,data,phy,0xf000,4K,
  factory,app,factory,0x10000,2M,
  spiffs,data,spiffs,0x210000,0x100000,
  ```

---

## 2. Adicionar o WAMR | Add WAMR

* Adicione ao arquivo `idf_component.yml`:
  Add to `idf_component.yml`:

  ```yaml
  dependencies:
    wasm-micro-runtime:
      version: "^1"
    idf:
      version: ">=4.4"
    espressif/esp32-camera:
      version: "*"
  ```

> O ESP-IDF buscará as dependências automaticamente durante o build.
> ESP-IDF will automatically fetch dependencies during the build.
> Não é necessário clonar o WAMR manualmente.
> Manual cloning of WAMR is not required.

---

## 3. Compilar, Gravar e Monitorar o Firmware

## 3. Build, Flash and Monitor the Firmware

Execute no terminal ESP-IDF:
Run in ESP-IDF terminal:

```bash
idf.py set-target esp32
idf.py fullclean
idf.py build
idf.py flash monitor
```

O comando `monitor` permite ver a saída da placa.
The `monitor` command shows the board's output in real time.

---

## ✅ Requisitos | Requirements

* **Placa / Board:** ESP32-CAM com PSRAM
* **Sistema / System:** Windows (recomendado / recommended)
* **Ferramentas / Tools:**

  * Git
  * Python 3.8+
  * `idf.py` no PATH / in PATH

---

## 🔧 Comandos `wat2wasm` e `xxd`

## 🔧 `wat2wasm` and `xxd` Commands

* Baixe o WABT (para `wat2wasm`):
  Download WABT here:
  [https://github.com/WebAssembly/wabt/releases](https://github.com/WebAssembly/wabt/releases)

* Baixe o `xxd`:
  Download `xxd` for Windows:
  [https://sourceforge.net/projects/xxd-for-windows/](https://sourceforge.net/projects/xxd-for-windows/)

* Adicione ambos ao PATH do sistema.
  Add both to system PATH.

* Converter `.wat` para `.wasm`:
  Convert `.wat` to `.wasm`:

  ```sh
  wat2wasm .\hello_word.wat -o .\hello_word.wasm
  ```

* Converter `.wasm` para array C:
  Convert `.wasm` to C array:

  ```sh
  xxd -i hello_word.wasm > test_wasm.h
  ```

---

## 🌐 Variáveis de Ambiente | Environment Variables

```sh
IDF_PATH: C:\Espressif\frameworks\esp-idf-v5.3.1\
PATH:
    C:\Espressif\tools\idf-exe\1.0.3\
    C:\xxd
    C:\Program Files (x86)\WABT\bin
```

---

## 📎 Observação | Note

Este projeto assume que o firmware suporta execução de WebAssembly via WAMR.
This project assumes that the firmware supports WebAssembly execution through WAMR.

Certifique-se de que o código WebAssembly pode lidar com os buffers de imagem capturados pela câmera.
Make sure your WebAssembly code can handle the image buffers captured by the camera.

```

---
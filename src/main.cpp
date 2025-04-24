#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FS.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>  // Para decodificar JPEG

#define MIN_BRIGHT_DISPLAY_ON 4
#define MIN_BRIGHT_DISPLAY_OFF 0
#define ESP32_LED_BUILTIN 2

MatrixPanel_I2S_DMA *dma_display = nullptr;
const char *logos[] = {"/logo1.jpg", "/logo2.jpg"};  // Archivos en SPIFFS
int currentLogo = 0;
unsigned long lastChangeTime = 0;
bool showFractals = false;  // Controla si mostrar fractales después de logo2.jpg
int fractalIndex = 0;       // Índice del fractal actual

// Callback para dibujar la imagen JPEG
bool tjpgDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    if (y >= 64) return false;
    dma_display->drawRGBBitmap(x, y, bitmap, w, h);
    return true;
}

void displaySetup(uint8_t displayBright, uint8_t displayRotation)
{
    HUB75_I2S_CFG mxconfig(64, 64, 1);

    // ✅ Corrección de pines de colores
    mxconfig.gpio.r1 = 25;
    mxconfig.gpio.g1 = 26;  // Verde correcto
    mxconfig.gpio.b1 = 27;  // Azul correcto
    mxconfig.gpio.r2 = 14;
    mxconfig.gpio.g2 = 12;  // Verde correcto
    mxconfig.gpio.b2 = 13;  // Azul correcto

    mxconfig.gpio.a = 23;
    mxconfig.gpio.b = 19;
    mxconfig.gpio.c = 5;
    mxconfig.gpio.d = 17;
    mxconfig.gpio.e = 18;
    mxconfig.gpio.clk = 16;
    mxconfig.gpio.lat = 4;
    mxconfig.gpio.oe = 15;

    mxconfig.clkphase = false;

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness8(displayBright);
    dma_display->clearScreen();
    dma_display->setRotation(displayRotation);
}

// Función para listar archivos en SPIFFS
void listarArchivosSPIFFS()
{
    Serial.println("\n📂 Archivos en SPIFFS:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        Serial.printf("📄 %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

// Cargar y mostrar imagen desde SPIFFS
void mostrarImagen(const char *filename)
{
    Serial.printf("🔍 Buscando archivo: %s\n", filename);

    if (!SPIFFS.exists(filename))
    {
        Serial.println("❌ Archivo no encontrado en SPIFFS.");
        return;
    }

    Serial.println("✅ Archivo encontrado, mostrando en la pantalla...");

    File file = SPIFFS.open(filename, "r");
    if (!file)
    {
        Serial.println("❌ Error al abrir el archivo.");
        return;
    }

    TJpgDec.drawFsJpg(0, 0, filename);
    file.close();
}

// Función para generar un fractal de Mandelbrot con parámetros personalizados
void generarFractalMandelbrot(float offsetX, float offsetY, float scale, int maxIter, uint16_t colorBase)
{
    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            float zx = 0, zy = 0;
            float cx = (x - 32) * scale + offsetX;
            float cy = (y - 32) * scale + offsetY;
            int iter = maxIter;

            while (zx * zx + zy * zy < 4 && iter > 0)
            {
                float tmp = zx * zx - zy * zy + cx;
                zy = 2 * zx * zy + cy;
                zx = tmp;
                iter--;
            }

            // Mapear el número de iteraciones a un color
            uint16_t color = dma_display->color565(
                (iter * 2) % 256,  // R
                (iter * 3) % 256,  // G
                (iter * 5) % 256   // B
            );
            dma_display->drawPixel(x, y, color);
        }
    }
}

// Función para mostrar un fractal específico
void mostrarFractal(int index)
{
    switch (index)
    {
    case 0:
        // Fractal 1: Región central
        generarFractalMandelbrot(-0.7, 0.0, 0.006, 100, dma_display->color565(255, 0, 0));
        break;
    case 1:
        // Fractal 2: Región con más detalles
        generarFractalMandelbrot(-0.5, 0.6, 0.003, 200, dma_display->color565(0, 255, 0));
        break;
    case 2:
        // Fractal 3: Otra región interesante
        generarFractalMandelbrot(-1.0, 0.3, 0.002, 300, dma_display->color565(0, 0, 255));
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(ESP32_LED_BUILTIN, OUTPUT);

    Serial.println("\n🔄 Iniciando SPIFFS...");
    if (!SPIFFS.begin(true))
    {
        Serial.println("❌ Error al inicializar SPIFFS.");
        return;
    }
    Serial.println("✅ SPIFFS inicializado correctamente.");

    listarArchivosSPIFFS();  // Muestra los archivos disponibles en SPIFFS

    displaySetup(50, 0);

    // Configurar decodificador de JPEG
    TJpgDec.setJpgScale(1); // Escala 1:1 (tamaño original)
    TJpgDec.setCallback(tjpgDrawCallback);

    // Mostrar el primer logo al iniciar
    mostrarImagen(logos[currentLogo]);
    lastChangeTime = millis();
}

void loop()
{
    if (!showFractals)
    {
        // Mostrar logos
        if (millis() - lastChangeTime >= 15000)
        {
            currentLogo = (currentLogo + 1) % 2;  // Alternar entre 0 y 1
            dma_display->clearScreen();
            mostrarImagen(logos[currentLogo]);
            lastChangeTime = millis();

            // Después de logo2.jpg, mostrar fractales
            if (currentLogo == 1)
            {
                showFractals = true;
                fractalIndex = 0;
            }
        }
    }
    else
    {
        // Mostrar fractales de Mandelbrot
        if (millis() - lastChangeTime >= 15000)
        {
            dma_display->clearScreen();
            mostrarFractal(fractalIndex);
            lastChangeTime = millis();
            fractalIndex++;

            // Después de 3 fractales, volver a los logos
            if (fractalIndex >= 3)
            {
                showFractals = false;
                currentLogo = 0;
                dma_display->clearScreen();
                mostrarImagen(logos[currentLogo]);
                lastChangeTime = millis();
            }
        }
    }
}
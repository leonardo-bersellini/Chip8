#include <iostream>
#include <array>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <random>
#include <windows.h>

#include "ui/renderer.h"

#ifdef DEBUG_MACRO
    #define DLog(...) do {std::cout << "[DEBUG] " << __VA_ARGS__ << std::endl;} while(0)
#else 
    #define DLog(...) do {} while (0)
#endif

/*
    > PROGETTO CHIP-8
    Chip-8 è una macchina-virtuale / emulatore di videogiochi degli anni '70 '80.
    Questo progetto mira ad imitare l'interprete chip-8, di fatto ricreando un codice che
    gestisce memoria e ciclo di esecuzione stile cpu.

    L'interprete permette di caricare un codice binario ROM (.chp8) ed eseguirlo in locale.
    Il programma viene storicamente allocato a aprtire dal byte 512, che in teoria sarebbe
    dedicato all'iterprete stesso.
*/

/* numero di celle da 8bit che costituiscono la memoria */
static const std::uint16_t memory_size = 4096;
/* lunghezza dello spazio di memoria dedicato al programma stesso*/
static const std::uint16_t memory_start = 512;

/* cella di partenza convenzionale per i dati del font nativo */
static const std::uint16_t font_start = 0x50; //80
/* array di 80 byte con i dati del font nativo */
static const std::array<std::uint8_t, 80> font_data = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/* seed del generatore casuale di numeri */
static std::random_device rd;

/* inizializzazione del generatore casuale di numeri */
static std::mt19937 gen(rd());

/* array di memoria per lo stato dei tatsi premuti */
static std::array<InputKey, 16> keyboard_keys;

/* */
static Renderer renderer;


class Stack_chip8 
{
public:
    void push(std::uint16_t value) {
        if(sp >= 16) return;
        buffer[sp++] = value;
    }

    std::uint16_t pop() {
        if(sp <= 0) return -1;
        return buffer[--sp];
    }

private:
    // stack a 16 livelli
    std::array<std::uint16_t, 16> buffer;
    // stack pointer, indica il livello corrente dello stack
    std::uint8_t sp = 0;
};

/* --- COMPONENTI CHIP-8 --- */

// memoria fisica allocabile del programma
static std::array<std::uint8_t, memory_size> memory;

// 16 registri per le operazioni del chip (V0 - VF)
static std::array<std::uint8_t, 16> registri; 

// registro indice, punta a indirizzi di memoria
static std::uint16_t I = 0;

// program counter, punta alla prossima operazione da eseguire
static std::uint16_t PC = 0;

// stack, permette di salvare il pc durante una subroutine (16 livelli)
static Stack_chip8 stack;

// delay timer, 60Hz
static std::uint8_t DTimer;

// sound timer, 60Hz
static std::uint8_t STimer;

// schermo, griglia di 64x32 px
static std::array<std::array<bool, 64>, 32> display;


/* --- UTILITIES E FUNZIONI --- */

std::string toHex(uint8_t byte) {
    std::stringstream ss;
    ss << "0x"
       << std::hex
       << std::setw(4)
       << std::setfill('0')
       << static_cast<int>(byte);

    return ss.str();
}

void loadFont() 
{
    for(int i=0; i < font_data.size(); ++i) 
    {
        memory[font_start + i] = font_data.at(i);
    }
}

void loadROM(const std::string& path) 
{
    std::ifstream file(path, std::ios::in | std::ios::binary);

    if(!file.is_open()) {
        std::cout << "errore nell'apertura del file, impossibile eseguire il load" << std::endl;
        return;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if(size > (memory_size - memory_start)) {
        std::cout << "file rom troppo grande, impossibile eseguire il load" << std::endl;
        return;
    }

    std::vector<std::uint8_t> rom(size);

    file.read(reinterpret_cast<char*>(rom.data()), size);

    std::uint16_t index = memory_start;
    for(const auto& t : rom) {
        memory[index] = t;
        std::cout << toHex(t) << std::endl;
        index++;
    }
}

void update_timers()
{
    const auto frequence = std::chrono::milliseconds(16); // 60Hz

    while(true) 
    {
        if(DTimer > 0) DTimer--;
        if(STimer > 0) STimer--;

        std::this_thread::sleep_for(frequence);
    }
}

/* --- OPERAZIONI DEL LOOP DI ESECUZIONE --- */

/* FETCH : lettura dell'operazione da eseguire */

std::uint16_t fetch() {
    const std::uint8_t left = memory[PC];
    const std::uint8_t right = memory[PC+1];
    //operazioni binarie, permettono di unire i due byte in uno solo.
    //op<< : sposta gli 8 bit nelle posizioni a destra, lasciando 0 dove vuoto
    //op | : sostituisce gli zeri in fondo con gli 8 bit del secondo valore
    auto result = (left << 8) | right; 
    return static_cast<std::uint16_t>(result);
}

/* DECODE : decodifica del significato dell'istruzione */

struct InstructionData {
    std::uint8_t op;
    std::uint8_t x;
    std::uint8_t y;
    std::uint8_t n;
    std::uint8_t nn;
    std::uint16_t nnn;
};

InstructionData decode(std::uint16_t instruction) 
{
    InstructionData data;

    //nibble 1: operatore, indica il tipo di operazione
    data.op = instruction >> 12;
    //nibble 2
    data.x = (instruction >> 8) & 0x0F; 
    //nibble 3
    data.y = (instruction >> 4) & 0x0F;
    //nibble 4
    data.n = instruction & 0x0F;
    //ultimi 8 bit
    data.nn = instruction & 0x00FF;
    //ultimi 12bit
    data.nnn = instruction & 0x0FFF;

    return data;
}

/* EXECUTE : esegue l'istruzione corrente */

namespace operation {
    const auto jump = 0x1;
    const auto load_to_register = 0x6;
    const auto sum_to_register = 0x7;
    const auto set_I = 0xA;
    const auto call_subroutine = 0x2;
    const auto jump_if = 0x3;
    const auto jump_if_different = 0x4;
    const auto random_number = 0xC;
    const auto draw_sprite = 0xD;

    //famiglia di sub operazioni
    const auto _0x0_ = 0x0;
    //namespace di operazioni 
    namespace system {
        const auto clear = 0xE0;
        const auto return_subroutine = 0xEE;
    }

    //famiglia con sub operazioni
    const auto _0x8_ = 0x8;
    //namespace per operazioni aritmetiche
    namespace math {
        const auto assign = 0x0;
        const auto or_op = 0x1;
        const auto and_op = 0x2;
        const auto xor_op = 0x3;
        const auto add = 0x4;
        const auto sub = 0x5;
    }

    //famiglia con sub operazioni
    const auto _0xE_ = 0xE;
    //namespace per tasti permuti e input operations
    namespace keyboard {
        const auto jump_if_pressed = 0x9E;
        const auto jump_ifnot_pressed = 0xA1;
    }

    //famiglia con sub operazioni 
    const auto _0xF_ = 0xF; 
    //namespace con operazioni varie "misc/memory operations"
    namespace misc {
        const auto get_delay_timer = 0x07;
        const auto set_delay_timer = 0x15;
        const auto set_sound_timer = 0x18;
        const auto add_to_index = 0x1E; 
        const auto bcd_conversion = 0x33; //binary-coded decimal
        const auto load_registers = 0x65;
        const auto load_font = 0x29;
    }
}

void execute(const InstructionData& data) 
{
    switch(data.op) 
    {
        case operation::jump:
            PC = data.nnn;
            break;

        case operation::load_to_register:
            registri[data.x] = data.nn;
            break;

        case operation::sum_to_register:
            registri[data.x] += data.nn;
            break;

        case operation::set_I:
            I = data.nnn;
            break;

        case operation::call_subroutine:
            stack.push(PC);
            PC = data.nnn;
            break;

        case operation::jump_if:
            if(registri[data.x] == data.nn) {
                PC += 2;
            }
            break;

        case operation::jump_if_different:
            if(registri[data.x] != data.nn) {
                PC += 2;
            }
            break;

        case operation::random_number:
        {
            std::uniform_int_distribution<int> dist(0, 255);
            int numero_rand = dist(gen);
            registri[data.x] = numero_rand & data.nn;
            break;
        }

        case operation::draw_sprite:
        {   
            /*
            L'operazione di draw avviene su sprite sempre larghi 8px e alti N.
            Questo draw avviene a partire dalla posizione (Vx, Vy).
            Il disegno avviene in XOR (tramite negazione bit per bit) sui pixel già 
            presenti a schermo. Questo permette di "cancellare" uno sprite 
            ridisegnandolo nella stessa posizione.
            */
                    
            //calcolo posizione di partenza e controllo overflow dello schermo
            std::uint8_t x_start = registri[data.x] % 64;
            std::uint8_t y_start = registri[data.y] % 32;

            // Vf è il flag di collisione
            registri[0xF] = 0;

            //operazione di disegno dello sprite (largo 8px e alto N)
            for(int row = 0; row < data.n; ++row) 
            {
                std::uint8_t sprite_byte = memory[I + row];

                for(int col = 0; col < 8; ++col) {
                    bool sprite_pixel = (sprite_byte >> (7 - col)) & 0x1;
                    auto posX = (x_start + col) % 64;
                    auto posY = (y_start + row) % 32;

                    if(sprite_pixel) {
                        if(display[posY][posX]) {
                            registri[0xF] = 1; //collisione
                        }
                        display[posY][posX] = !display[posY][posX];
                    }
                }
            }
            break;
        }

        case operation::_0x0_:
            switch(data.nn)
            {
                case operation::system::clear:
                    for (auto& row : display) {
                        row.fill(false);
                    }
                    break;

                case operation::system::return_subroutine:
                    PC = stack.pop();
                    break;
                
                default:
                    DLog("[execute][err] istruzione di tipo -0x0- sconosciuta code: " << toHex(data.n));
                    break;
            }
            break;

        case operation::_0x8_:
            switch(data.n) 
            {
                case operation::math::assign:
                    registri[data.x] = registri[data.y];
                    break;

                case operation::math::or_op:
                    registri[data.x] = registri[data.x] | registri[data.y];
                    break;

                case operation::math::and_op:
                    registri[data.x] = registri[data.x] & registri[data.y];
                    break;

                case operation::math::xor_op:
                    registri[data.x] = registri[data.x] ^ registri[data.y];
                    break;

                case operation::math::add:
                {
                    std::uint16_t sum = registri[data.x] + registri[data.y];
                    registri[0xF] = (sum > 255) ? 1 : 0;
                    registri[data.x] = static_cast<std::uint8_t>(sum);
                    break;
                }

                case operation::math::sub:
                    registri[0xF] = (registri[data.x] >= registri[data.y]) ? 1 : 0;
                    registri[data.x] = registri[data.x] - registri[data.y];
                    break;

                default:
                    DLog("[execute] [err]: operazione di tipo -0x8- sconosciuta. code: " << toHex(data.n));
                    return;
                    break;
            }
            break;

        case operation::_0xE_:
            switch(data.nn)
            {
                case operation::keyboard::jump_if_pressed:
                    if(renderer.isPressed(registri[data.x])) {
                        PC += 2;
                    }
                    break;

                case operation::keyboard::jump_ifnot_pressed:
                    if(!renderer.isPressed(registri[data.x])) {
                        PC += 2;
                    }
                    break;

                default:
                    DLog("[execute] [err]: operazione di tipo -0xE- sconosciuta. code: " << toHex(data.nn));
                    return;
                    break;
            }
            break;

        case operation::_0xF_:
            switch(data.nn) 
            {
                case operation::misc::get_delay_timer:
                    registri[data.x] = DTimer;
                    break;

                case operation::misc::set_delay_timer:
                    DTimer = registri[data.x];
                    break;

                case operation::misc::set_sound_timer:
                    STimer = registri[data.x];
                    break;

                case operation::misc::add_to_index:
                    I += registri[data.x];
                    break;

                case operation::misc::bcd_conversion:
                {
                    //converte un numero a 3 cifre in 3 numeri
                    auto vx = registri[data.x];
                    memory[I]   = vx / 100;        // k
                    memory[I+1] = (vx / 10) % 10;  // d
                    memory[I+2] = vx % 10;         // u
                    break;
                }

                case operation::misc::load_registers:
                {
                    //carica nei registri i valori in memoria dopo I
                    for(int i=0; i <= data.x; ++i) {
                        registri[i] = memory[I + i];
                    }
                    break;
                }

                case operation::misc::load_font:
                {
                    auto requested_char = registri[data.x];
                    //calcola l'indirizzo del carattere richiesto, considerando 5 byte per ognuno
                    I = font_start + (5 * requested_char);
                    break;
                }

                default:
                    DLog("[execute] [err]: operazione di tipo -0xF- sconosciuta. code: " << toHex(data.nn));
                    return;
                    break;
            }
            break;

        default:
            DLog("[execute] [err]: operazione non conosciuta. op: " << toHex(data.op));
            return;
            break;
    }
    DLog("[execute] operazione eseguita: " << toHex(data.op));
}

/* --- MAIN --- */

int main(int argc, char* argv[]) 
{

#ifdef DEBUG_MACRO
    std::cout << "[CHIP8 DEBUG MODE]" << "\n" << std::endl; 
#else
    std::cout << "[CHIP8 RELEASE MODE]" << "\n" << std::endl;
#endif

    // Const Declarations

    const auto loop_frequence = std::chrono::milliseconds(10);    // 100Hz
    std::thread timer(update_timers);

    // Load ROM

    if(argc < 2) {
        std::string rom;
        std::cout << "Seleziona il path per il file ROM: ";
        std::cin >> rom;
        loadROM(rom);
    } else {
        loadROM(argv[1]);
    }
    
    // Inizializzazione

#ifdef RELEASE_MACRO
    //nasconde la console dopo aver letto il path rom se in release
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

    loadFont();
    PC = memory_start;
    timer.detach();

    // UI Sfml

    renderer.showWindow();

    // Main Loop

    while(true) 
    {
        const auto instruction = fetch();
        PC += 2;
        const auto operation = decode(instruction);

        execute(operation);

        // esecuzione window sfml
        renderer.executeWindowFrame();
        renderer.updateDisplayState(display);

        if(!renderer.isOpen()) return 0;

        std::this_thread::sleep_for(loop_frequence);
    }

    return 0;
}
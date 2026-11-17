/**
 * PROYECTO DE ESTRUCTURAS DE DATOS - ENTREGA PARCIAL 1
 * Simulador de Civilizaciones (God Game) con Interfaz Gráfica Web Embebida
 *
 * Arquitectura:
 * - Motor Lógico en C++17: Ejecuta todas las estructuras de datos en bajo nivel.
 *   1. std::vector<Cell> y std::vector<Country>: Mapa 2D en memoria contigua (O(1)) y naciones.
 *   2. std::queue<RoutineTask>: Cola FIFO para rolls demográficos y científicos por Tick (O(1)).
 *   3. std::priority_queue<PriorityEvent>: Max-Heap para arbitraje de conflictos y milagros (O(log K)).
 *   4. std::stack<WorldSnapshot> y std::stack<std::string>: Historial Undo y pila de navegación LIFO (O(1)).
 * - Interfaz Gráfica (GUI): Servidor HTTP en sockets nativos (puerto 8080) que sirve un lienzo
 *   interactivo en HTML5 Canvas conectado en tiempo real con la memoria de C++.
 */

#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <string>
#include <sstream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif

// ============================================================================
// 1. DEFINICIÓN DE ENUMERACIONES Y MODELOS DE DATOS (TDA)
// ============================================================================

enum class Biome { OCEAN = 0, DESERT = 1, PLAINS = 2, FOREST = 3, MOUNTAIN = 4, TUNDRA = 5 };
enum class SettlementType { NONE = 0, VILLAGE = 1, CITY = 2, CAPITAL = 3 };
enum class Era { PRIMITIVE = 0, ANCIENT = 1, CLASSICAL = 2, INDUSTRIAL = 3, TECHNOLOGICAL = 4 };

enum class EventType {
    DIVINE_INTERVENTION = 100,
    PROPHET_APPEARANCE = 85,
    EPIDEMIC_OUTBREAK = 70,
    COLONIZATION_CLAIM = 50,
    TECH_BREAKTHROUGH = 30
};

struct Cell {
    int x = 0;
    int y = 0;
    Biome biome = Biome::OCEAN;
    int ownerCountryId = -1;
    SettlementType settlement = SettlementType::NONE;
    long population = 0;
    float birthRate = 0.028f;
    float mortalityRate = 0.014f;
    float religiosity = 0.25f;
    float happiness = 0.75f;
    float hunger = 0.10f;
    float education = 0.05f;
    float wealth = 10.0f;

    bool isHabitable() const { return biome != Biome::OCEAN; }
};

struct Country {
    int id;
    std::string name;
    std::string colorHex;
    Era currentEra = Era::PRIMITIVE;
    float techProgress = 0.0f;
    int controlledCells = 0;
    std::vector<std::string> milestones;
};

struct RoutineTask {
    int cellIndex;
};

struct PriorityEvent {
    EventType type;
    int weight;
    int targetX;
    int targetY;
    int countryId;
    std::string description;

    // Ordenamiento para Max-Heap en std::priority_queue (Complejidad O(1))
    bool operator<(const PriorityEvent& other) const {
        if (weight != other.weight) return weight < other.weight;
        return countryId > other.countryId;
    }
};

struct WorldSnapshot {
    int tick;
    std::vector<Cell> grid;
    std::vector<Country> countries;
    std::vector<std::string> logs;
};

// ============================================================================
// 2. MOTOR DE SIMULACIÓN BASADO EN ESTRUCTURAS DE DATOS LINEALES
// ============================================================================

class GodGameEngine {
private:
    int width;
    int height;
    unsigned int seed;
    int currentTick;
    std::mt19937 rng;

    // 1. Arreglos Dinámicos (std::vector)
    std::vector<Cell> grid;
    std::vector<Country> countries;
    std::vector<std::string> eventLog;

    // 2. Cola FIFO (std::queue)
    std::queue<RoutineTask> routineQueue;
    int lastRoutineProcessedCount = 0;

    // 3. Cola de Prioridad (std::priority_queue)
    std::priority_queue<PriorityEvent> priorityQueue;
    int lastPriorityProcessedCount = 0;

    // 4. Pilas LIFO (std::stack)
    std::stack<WorldSnapshot> undoStack;
    std::stack<std::string> uiViewStack;

public:
    GodGameEngine(int w, int h, unsigned int initialSeed)
        : width(w), height(h), seed(initialSeed), currentTick(0), rng(initialSeed) {
        uiViewStack.push("VISTA_POLITICA");
        resetWorld(initialSeed);
    }

    inline int idx(int x, int y) const { return y * width + x; }
    inline bool valid(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }

    void resetWorld(unsigned int newSeed) {
        seed = newSeed;
        currentTick = 0;
        rng.seed(seed);
        grid.clear();
        countries.clear();
        eventLog.clear();
        while (!undoStack.empty()) undoStack.pop();
        while (!routineQueue.empty()) routineQueue.pop();
        while (!priorityQueue.empty()) priorityQueue.pop();

        grid.resize(width * height);
        std::uniform_real_distribution<float> noiseDist(0.0f, 1.0f);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Cell& c = grid[idx(x, y)];
                c.x = x;
                c.y = y;

                float nx = static_cast<float>(x) / width - 0.5f;
                float ny = static_cast<float>(y) / height - 0.5f;
                float radial = std::sqrt(nx * nx + ny * ny) * 1.75f;
                float wave = (std::sin(x * 0.35f + (seed % 100)) + std::cos(y * 0.35f + (seed % 77))) * 0.25f + 0.5f;
                float elev = wave - radial * 0.50f + noiseDist(rng) * 0.14f;

                if (elev < 0.16f) c.biome = Biome::OCEAN;
                else if (elev < 0.25f) c.biome = Biome::DESERT;
                else if (elev < 0.54f) c.biome = Biome::PLAINS;
                else if (elev < 0.72f) c.biome = Biome::FOREST;
                else if (elev < 0.85f) c.biome = Biome::MOUNTAIN;
                else c.biome = Biome::TUNDRA;
            }
        }

        struct CivPreset { std::string name; std::string color; };
        std::vector<CivPreset> presets = {
            {"Imperio Aethel", "#3b82f6"},
            {"Republica Boreal", "#10b981"},
            {"Dinastia Solaris", "#f59e0b"},
            {"Confederacion Valka", "#ec4899"}
        };

        std::uniform_int_distribution<int> rxDist(2, width - 3);
        std::uniform_int_distribution<int> ryDist(2, height - 3);

        for (size_t i = 0; i < presets.size(); ++i) {
            for (int attempt = 0; attempt < 200; ++attempt) {
                int rx = rxDist(rng);
                int ry = ryDist(rng);
                Cell& startCell = grid[idx(rx, ry)];
                if (startCell.isHabitable() && startCell.ownerCountryId == -1) {
                    Country nation;
                    nation.id = static_cast<int>(i);
                    nation.name = presets[i].name;
                    nation.colorHex = presets[i].color;
                    nation.controlledCells = 1;
                    nation.milestones.push_back("El Primer Fuego");
                    countries.push_back(nation);

                    startCell.ownerCountryId = nation.id;
                    startCell.settlement = SettlementType::CAPITAL;
                    startCell.population = 1500;
                    startCell.wealth = 60.0f;
                    break;
                }
            }
        }

        addLog("Mundo inicializado con semilla determinista " + std::to_string(seed) + ".");
        saveStateToStack();
    }

    void addLog(const std::string& msg) {
        eventLog.push_back("[Tick " + std::to_string(currentTick) + "] " + msg);
        if (eventLog.size() > 40) {
            eventLog.erase(eventLog.begin());
        }
    }

    void saveStateToStack() {
        WorldSnapshot snap;
        snap.tick = currentTick;
        snap.grid = grid;
        snap.countries = countries;
        snap.logs = eventLog;
        undoStack.push(snap);
    }

    bool undoTick() {
        if (undoStack.size() <= 1) return false;
        undoStack.pop();
        const WorldSnapshot& prev = undoStack.top();
        currentTick = prev.tick;
        grid = prev.grid;
        countries = prev.countries;
        eventLog = prev.logs;
        addLog("Estado revertido mediante std::stack<WorldSnapshot> (LIFO).");
        return true;
    }

    void pushUiView(const std::string& viewName) {
        uiViewStack.push(viewName);
    }

    void popUiView() {
        if (uiViewStack.size() > 1) uiViewStack.pop();
    }

    std::string getEraLabel(Era e) const {
        switch (e) {
            case Era::PRIMITIVE: return "1. Era Primitiva";
            case Era::ANCIENT: return "2. Era Antigua";
            case Era::CLASSICAL: return "3. Edad Clasica y Feudal";
            case Era::INDUSTRIAL: return "4. Era Industrial";
            case Era::TECHNOLOGICAL: return "5. Era Tecnologica";
        }
        return "Desconocida";
    }

    std::string getEraMilestone(Era e) const {
        switch (e) {
            case Era::PRIMITIVE: return "Dominio de la Piedra";
            case Era::ANCIENT: return "Invencion de la Escritura y Codigo de Leyes";
            case Era::CLASSICAL: return "Rutas Comerciales y Arquitectura Monumental";
            case Era::INDUSTRIAL: return "Maquina de Vapor y Metodo Cientifico";
            case Era::TECHNOLOGICAL: return "Red Global, Genetica y Energia Sostenible";
        }
        return "Avance Civilizatorio";
    }

    void advanceTick() {
        saveStateToStack();
        currentTick++;
        lastRoutineProcessedCount = 0;
        lastPriorityProcessedCount = 0;

        // 1. Encolar todas las celdas habitadas en la Cola FIFO (std::queue)
        for (size_t i = 0; i < grid.size(); ++i) {
            if (grid[i].ownerCountryId != -1 && grid[i].population > 0) {
                routineQueue.push({static_cast<int>(i)});
            }
        }

        std::uniform_real_distribution<float> prob(0.0f, 1.0f);

        // 2. Desencolar y procesar acciones automáticas en orden FIFO O(1)
        while (!routineQueue.empty()) {
            RoutineTask task = routineQueue.front();
            routineQueue.pop();
            lastRoutineProcessedCount++;

            Cell& c = grid[task.cellIndex];
            Country& nation = countries[c.ownerCountryId];

            float growth = c.birthRate - c.mortalityRate - (c.hunger * 0.015f);
            c.population = std::max(50L, c.population + static_cast<long>(c.population * growth));
            c.wealth += (c.population * 0.002f) * (1.0f + c.education);

            if (c.population > 6000 && c.settlement == SettlementType::VILLAGE) {
                c.settlement = SettlementType::CITY;
                c.education = std::min(1.0f, c.education + 0.12f);
            } else if (c.population > 900 && c.settlement == SettlementType::NONE) {
                c.settlement = SettlementType::VILLAGE;
            }

            // Roll de Avance Científico
            if (prob(rng) < (0.08f + c.education * 0.25f)) {
                nation.techProgress += 4.2f;
                if (nation.techProgress >= 100.0f && nation.currentEra != Era::TECHNOLOGICAL) {
                    priorityQueue.push({
                        EventType::TECH_BREAKTHROUGH,
                        static_cast<int>(EventType::TECH_BREAKTHROUGH),
                        c.x, c.y, nation.id,
                        nation.name + " completa los requisitos para cambiar de Era."
                    });
                }
            }

            // Roll de Epidemia
            if (c.population > 8500 && prob(rng) < 0.03f) {
                priorityQueue.push({
                    EventType::EPIDEMIC_OUTBREAK,
                    static_cast<int>(EventType::EPIDEMIC_OUTBREAK),
                    c.x, c.y, nation.id,
                    "Epidemia reportada en ciudad de " + nation.name + " (" + std::to_string(c.x) + "," + std::to_string(c.y) + ")."
                });
            }

            // Roll de Colonización de celdas vecinas
            if (c.population > 700 && prob(rng) < 0.24f) {
                const int dx[4] = {0, 0, 1, -1};
                const int dy[4] = {1, -1, 0, 0};
                int dir = static_cast<int>(prob(rng) * 4) % 4;
                int nx = c.x + dx[dir];
                int ny = c.y + dy[dir];

                if (valid(nx, ny)) {
                    Cell& neighbor = grid[idx(nx, ny)];
                    if (neighbor.isHabitable() && neighbor.ownerCountryId == -1) {
                        int dynamicWeight = static_cast<int>(EventType::COLONIZATION_CLAIM) + static_cast<int>(c.wealth / 15.0f);
                        priorityQueue.push({
                            EventType::COLONIZATION_CLAIM,
                            dynamicWeight,
                            nx, ny, nation.id,
                            nation.name + " coloniza el territorio (" + std::to_string(nx) + "," + std::to_string(ny) + ")."
                        });
                    }
                }
            }
        }

        // 3. Procesar la Cola de Prioridad (std::priority_queue) en O(log K)
        while (!priorityQueue.empty()) {
            PriorityEvent ev = priorityQueue.top();
            priorityQueue.pop();
            lastPriorityProcessedCount++;

            Cell& target = grid[idx(ev.targetX, ev.targetY)];

            if (ev.type == EventType::COLONIZATION_CLAIM) {
                if (target.ownerCountryId == -1) {
                    target.ownerCountryId = ev.countryId;
                    target.population = 320;
                    target.settlement = SettlementType::VILLAGE;
                    countries[ev.countryId].controlledCells++;
                    addLog(ev.description);
                }
            } else if (ev.type == EventType::EPIDEMIC_OUTBREAK) {
                long casualties = static_cast<long>(target.population * 0.28f);
                target.population -= casualties;
                target.happiness = std::max(0.15f, target.happiness - 0.18f);
                addLog(ev.description + " Bajas: " + std::to_string(casualties) + ".");
            } else if (ev.type == EventType::TECH_BREAKTHROUGH) {
                Country& nat = countries[ev.countryId];
                if (nat.techProgress >= 100.0f && nat.currentEra != Era::TECHNOLOGICAL) {
                    nat.techProgress = 0.0f;
                    nat.currentEra = static_cast<Era>(static_cast<int>(nat.currentEra) + 1);
                    std::string m = getEraMilestone(nat.currentEra);
                    nat.milestones.push_back(m);
                    addLog("HITO: " + nat.name + " avanza a " + getEraLabel(nat.currentEra) + " (" + m + ").");
                }
            } else {
                addLog(ev.description);
            }
        }
    }

    void applyDivineAction(const std::string& type, int cx, int cy, int radius) {
        if (!valid(cx, cy)) return;
        saveStateToStack();

        int affected = 0;
        std::uniform_real_distribution<float> prob(0.0f, 1.0f);

        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (valid(x, y) && ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius)) {
                    Cell& c = grid[idx(x, y)];
                    if (!c.isHabitable()) continue;
                    affected++;

                    if (type == "blessing") {
                        c.hunger = 0.0f;
                        c.happiness = std::min(1.0f, c.happiness + 0.30f);
                        c.population = static_cast<long>(c.population * 1.25f) + 150;
                        c.wealth += 30.0f;
                    } else if (type == "cataclysm") {
                        c.population = static_cast<long>(c.population * 0.35f);
                        c.hunger = std::min(1.0f, c.hunger + 0.45f);
                        c.happiness = std::max(0.05f, c.happiness - 0.35f);
                        if (c.population < 80 && c.ownerCountryId != -1 && c.settlement != SettlementType::CAPITAL) {
                            countries[c.ownerCountryId].controlledCells--;
                            c.ownerCountryId = -1;
                            c.settlement = SettlementType::NONE;
                            c.population = 0;
                        }
                    } else if (type == "prophet") {
                        if (c.ownerCountryId != -1) {
                            float chance = c.religiosity + (1.0f - c.happiness) * 0.5f;
                            if (prob(rng) < chance + 0.3f) {
                                c.religiosity = std::min(1.0f, c.religiosity + 0.35f);
                                c.happiness = std::min(1.0f, c.happiness + 0.15f);
                            }
                        }
                    } else if (type == "science") {
                        if (c.ownerCountryId != -1) {
                            c.education = std::min(1.0f, c.education + 0.25f);
                            countries[c.ownerCountryId].techProgress = std::min(100.0f, countries[c.ownerCountryId].techProgress + 12.0f);
                        }
                    }
                }
            }
        }

        std::string label = "Intervencion Divina";
        if (type == "blessing") label = "Milagro de Abundancia";
        else if (type == "cataclysm") label = "Cataclismo Meteorico";
        else if (type == "prophet") label = "Envio de Iluminado";
        else if (type == "science") label = "Inspiracion Cientifica";

        addLog("DIOS ejecuto [" + label + "] en (" + std::to_string(cx) + "," + std::to_string(cy) +
               ") radio " + std::to_string(radius) + " (" + std::to_string(affected) + " celdas afectadas).");
    }

    // Serialización directa de las Estructuras de Datos en C++ a JSON para la GUI
    std::string serializeStateToJson() const {
        std::ostringstream ss;
        ss << "{";
        ss << "\"width\":" << width << ",";
        ss << "\"height\":" << height << ",";
        ss << "\"seed\":" << seed << ",";
        ss << "\"tick\":" << currentTick << ",";
        ss << "\"undoStackSize\":" << undoStack.size() << ",";
        ss << "\"uiStackSize\":" << uiViewStack.size() << ",";
        ss << "\"uiTop\":\"" << (uiViewStack.empty() ? "NONE" : uiViewStack.top()) << "\",";
        ss << "\"lastQueueProcessed\":" << lastRoutineProcessedCount << ",";
        ss << "\"lastPriorityProcessed\":" << lastPriorityProcessedCount << ",";

        ss << "\"countries\":[";
        for (size_t i = 0; i < countries.size(); ++i) {
            const Country& c = countries[i];
            if (i > 0) ss << ",";
            ss << "{"
               << "\"id\":" << c.id << ","
               << "\"name\":\"" << c.name << "\","
               << "\"color\":\"" << c.colorHex << "\","
               << "\"era\":\"" << getEraLabel(c.currentEra) << "\","
               << "\"tech\":" << c.techProgress << ","
               << "\"cells\":" << c.controlledCells << ","
               << "\"lastMilestone\":\"" << (c.milestones.empty() ? "" : c.milestones.back()) << "\""
               << "}";
        }
        ss << "],";

        ss << "\"logs\":[";
        int logStart = std::max(0, static_cast<int>(eventLog.size()) - 12);
        for (size_t i = logStart; i < eventLog.size(); ++i) {
            if (i > static_cast<size_t>(logStart)) ss << ",";
            ss << "\"" << eventLog[i] << "\"";
        }
        ss << "],";

        ss << "\"cells\":[";
        for (size_t i = 0; i < grid.size(); ++i) {
            const Cell& c = grid[i];
            if (i > 0) ss << ",";
            ss << "["
               << static_cast<int>(c.biome) << ","
               << c.ownerCountryId << ","
               << static_cast<int>(c.settlement) << ","
               << c.population << ","
               << static_cast<int>(c.happiness * 100) << ","
               << static_cast<int>(c.hunger * 100) << ","
               << static_cast<int>(c.religiosity * 100) << ","
               << static_cast<int>(c.education * 100) << ","
               << static_cast<int>(c.wealth)
               << "]";
        }
        ss << "]}";
        return ss.str();
    }
};

// ============================================================================
// 3. INTERFAZ GRÁFICA HTML5 CANVAS EMBEBIDA EN EL EJECUTABLE DE C++
// ============================================================================

const char* EMBEDDED_GUI_HTML = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<title>Simulador de Civilizaciones - Motor C++ Estructuras de Datos</title>
<style>
  :root {
    --bg: #0f172a; --panel: #1e293b; --border: #334155;
    --text: #f8fafc; --muted: #94a3b8; --accent: #38bdf8;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: system-ui, -apple-system, sans-serif; }
  body { background: var(--bg); color: var(--text); display: flex; flex-direction: column; height: 100vh; overflow: hidden; }
  header {
    background: var(--panel); border-bottom: 1px solid var(--border);
    padding: 10px 18px; display: flex; align-items: center; justify-content: space-between; gap: 12px;
  }
  .brand { font-weight: 700; font-size: 15px; color: var(--accent); }
  .controls { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
  button, select, input {
    background: #0f172a; color: var(--text); border: 1px solid var(--border);
    padding: 6px 12px; border-radius: 6px; font-size: 13px; cursor: pointer;
  }
  button:hover { border-color: var(--accent); background: #1e293b; }
  button.primary { background: #0284c7; border-color: #38bdf8; font-weight: 600; }
  button.primary:hover { background: #0369a1; }
  button.active-tool { border-color: #f59e0b; background: #451a03; color: #fde68a; }
  main { display: grid; grid-template-columns: 1fr 390px; flex: 1; overflow: hidden; }
  .canvas-wrap {
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    background: #090d16; position: relative; padding: 12px;
  }
  canvas { border: 1px solid var(--border); border-radius: 6px; cursor: crosshair; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }
  .toolbar {
    margin-top: 10px; display: flex; gap: 8px; align-items: center;
    background: var(--panel); padding: 8px 14px; border-radius: 8px; border: 1px solid var(--border);
  }
  .sidebar {
    background: var(--panel); border-left: 1px solid var(--border);
    padding: 14px; overflow-y: auto; display: flex; flex-direction: column; gap: 14px;
  }
  .card { background: #0f172a; border: 1px solid var(--border); border-radius: 8px; padding: 12px; }
  .card h3 { font-size: 12px; text-transform: uppercase; letter-spacing: 0.05em; color: var(--accent); margin-bottom: 8px; }
  .stat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 6px; font-size: 12px; }
  .stat-item { display: flex; justify-content: space-between; border-bottom: 1px solid #1e293b; padding-bottom: 3px; }
  .stat-label { color: var(--muted); }
  .civ-row { margin-bottom: 8px; padding-bottom: 8px; border-bottom: 1px solid #1e293b; font-size: 12px; }
  .civ-header { display: flex; justify-content: space-between; font-weight: 600; margin-bottom: 4px; }
  .bar-bg { height: 6px; background: #1e293b; border-radius: 3px; overflow: hidden; margin-top: 4px; }
  .bar-fill { height: 100%; transition: width 0.25s; }
  .log-list { font-size: 11.5px; color: #cbd5e1; max-height: 150px; overflow-y: auto; display: flex; flex-direction: column; gap: 4px; }
  .ds-badge { font-family: monospace; color: #38bdf8; font-size: 11.5px; }
</style>
</head>
<body>
<header>
  <div class="brand">GOD GAME SANDBOX — MOTOR NATIVO C++17</div>
  <div class="controls">
    <label style="font-size:12px;color:var(--muted);">Semilla:</label>
    <input type="number" id="seedInput" value="20261105" style="width:105px;">
    <button onclick="resetWorld()">Generar Mundo</button>
    <button class="primary" onclick="stepTick()">Avanzar 1 Tick</button>
    <button id="autoBtn" onclick="toggleAuto()">Auto: Pausado</button>
    <button onclick="undoAction()">Deshacer (std::stack)</button>
    <select id="viewSelect" onchange="changeView(this.value)">
      <option value="VISTA_POLITICA">Mapa Politico</option>
      <option value="VISTA_BIOMAS">Mapa de Biomas</option>
      <option value="VISTA_POBLACION">Densidad Demografica</option>
      <option value="VISTA_RELIGION">Fervor Religioso</option>
    </select>
  </div>
</header>

<main>
  <div class="canvas-wrap">
    <canvas id="mapCanvas" width="560" height="560"></canvas>
    <div class="toolbar">
      <span style="font-size:12px;color:var(--muted);">Herramienta Divina:</span>
      <button id="tool-inspect" class="active-tool" onclick="setTool('inspect')">Inspeccionar Celda</button>
      <button id="tool-blessing" onclick="setTool('blessing')">Lluvia de Abundancia</button>
      <button id="tool-cataclysm" onclick="setTool('cataclysm')">Cataclismo</button>
      <button id="tool-prophet" onclick="setTool('prophet')">Enviar Iluminado</button>
      <button id="tool-science" onclick="setTool('science')">Impulso Cientifico</button>
      <label style="font-size:12px;color:var(--muted);margin-left:6px;">Radio:</label>
      <input type="number" id="radiusInput" value="3" min="1" max="8" style="width:52px;">
    </div>
  </div>

  <div class="sidebar">
    <div class="card">
      <h3>Monitor de Estructuras de Datos (C++ Memory)</h3>
      <div class="stat-grid">
        <div class="stat-item"><span class="stat-label">Tick Actual:</span><strong id="stTick">0</strong></div>
        <div class="stat-item"><span class="stat-label">std::vector&lt;Cell&gt;:</span><span class="ds-badge" id="stVec">0</span></div>
        <div class="stat-item"><span class="stat-label">std::queue (FIFO):</span><span class="ds-badge" id="stQueue">0</span></div>
        <div class="stat-item"><span class="stat-label">std::priority_queue:</span><span class="ds-badge" id="stPrio">0</span></div>
        <div class="stat-item"><span class="stat-label">std::stack (Undo):</span><span class="ds-badge" id="stUndo">0</span></div>
        <div class="stat-item"><span class="stat-label">std::stack (UI):</span><span class="ds-badge" id="stUi">VISTA_POLITICA</span></div>
      </div>
    </div>

    <div class="card">
      <h3>Inspector de Celda — Acceso O(1)</h3>
      <div class="stat-grid" id="cellInspector">
        <div class="stat-item"><span class="stat-label">Coordenada:</span><span id="ciCoord">(0, 0)</span></div>
        <div class="stat-item"><span class="stat-label">Indice Vector:</span><span class="ds-badge" id="ciIdx">0</span></div>
        <div class="stat-item"><span class="stat-label">Bioma:</span><span id="ciBiome">-</span></div>
        <div class="stat-item"><span class="stat-label">Asentamiento:</span><span id="ciSettle">-</span></div>
        <div class="stat-item"><span class="stat-label">Poblacion:</span><span id="ciPop">0</span></div>
        <div class="stat-item"><span class="stat-label">Felicidad:</span><span id="ciHappy">0%</span></div>
        <div class="stat-item"><span class="stat-label">Hambre:</span><span id="ciHunger">0%</span></div>
        <div class="stat-item"><span class="stat-label">Religiosidad:</span><span id="ciRelig">0%</span></div>
        <div class="stat-item"><span class="stat-label">Educacion:</span><span id="ciEdu">0%</span></div>
        <div class="stat-item"><span class="stat-label">Riqueza:</span><span id="ciWealth">0</span></div>
      </div>
    </div>

    <div class="card">
      <h3>Progreso de las Civilizaciones</h3>
      <div id="civContainer"></div>
    </div>

    <div class="card" style="flex:1;">
      <h3>Bitacora de Eventos del Mundo</h3>
      <div class="log-list" id="logContainer"></div>
    </div>
  </div>
</main>

<script>
let state = null;
let currentTool = 'inspect';
let currentView = 'VISTA_POLITICA';
let selectedX = 0, selectedY = 0;
let hoverX = -1, hoverY = -1;
let autoTimer = null;

const canvas = document.getElementById('mapCanvas');
const ctx = canvas.getContext('2d');
const BIOME_COLORS = ['#0c4a6e', '#d97706', '#15803d', '#14532d', '#475569', '#cbd5e1'];
const BIOME_NAMES = ['Oceano', 'Desierto', 'Llanura', 'Bosque', 'Montana', 'Tundra'];
const SETTLE_NAMES = ['Ninguno', 'Pueblo', 'Ciudad', 'Capital'];

async function apiCall(path) {
  const res = await fetch(path, { cache: 'no-store' });
  state = await res.json();
  updateUI();
  drawMap();
}

function setTool(tool) {
  currentTool = tool;
  document.querySelectorAll('.toolbar button').forEach(b => b.classList.remove('active-tool'));
  document.getElementById('tool-' + tool).classList.add('active-tool');
  drawMap();
}

function changeView(view) {
  currentView = view;
  apiCall('/api/action?cmd=view&name=' + encodeURIComponent(view));
}

function stepTick() { apiCall('/api/action?cmd=tick'); }
function undoAction() { apiCall('/api/action?cmd=undo'); }
function resetWorld() {
  const s = document.getElementById('seedInput').value || 20261105;
  apiCall('/api/action?cmd=reset&seed=' + s);
}

function toggleAuto() {
  const btn = document.getElementById('autoBtn');
  if (autoTimer) {
    clearInterval(autoTimer);
    autoTimer = null;
    btn.textContent = 'Auto: Pausado';
  } else {
    autoTimer = setInterval(stepTick, 700);
    btn.textContent = 'Auto: Ejecutando';
  }
}

function updateUI() {
  if (!state) return;
  document.getElementById('stTick').textContent = state.tick;
  document.getElementById('stVec').textContent = state.cells.length + ' celdas';
  document.getElementById('stQueue').textContent = state.lastQueueProcessed + ' ops/tick';
  document.getElementById('stPrio').textContent = state.lastPriorityProcessed + ' eventos';
  document.getElementById('stUndo').textContent = state.undoStackSize + ' nodos';
  document.getElementById('stUi').textContent = state.uiTop;

  updateInspector(selectedX, selectedY);

  const civDiv = document.getElementById('civContainer');
  civDiv.innerHTML = '';
  state.countries.forEach(c => {
    civDiv.innerHTML += `
      <div class="civ-row">
        <div class="civ-header">
          <span style="color:${c.color}">${c.name}</span>
          <span>${c.cells} celdas</span>
        </div>
        <div style="color:var(--muted);display:flex;justify-content:space-between;">
          <span>${c.era}</span><span>Tech: ${c.tech.toFixed(1)}%</span>
        </div>
        <div style="font-size:11px;color:#cbd5e1;margin-top:2px;">Hito: ${c.lastMilestone}</div>
        <div class="bar-bg"><div class="bar-fill" style="width:${Math.min(100, c.tech)}%;background:${c.color};"></div></div>
      </div>`;
  });

  const logDiv = document.getElementById('logContainer');
  logDiv.innerHTML = state.logs.slice().reverse().map(l => `<div>${l}</div>`).join('');
}

function updateInspector(x, y) {
  if (!state || x < 0 || y < 0 || x >= state.width || y >= state.height) return;
  const index = y * state.width + x;
  const cell = state.cells[index];
  document.getElementById('ciCoord').textContent = `(${x}, ${y})`;
  document.getElementById('ciIdx').textContent = `grid[${index}]`;
  document.getElementById('ciBiome').textContent = BIOME_NAMES[cell[0]];
  document.getElementById('ciSettle').textContent = SETTLE_NAMES[cell[2]];
  document.getElementById('ciPop').textContent = cell[3].toLocaleString();
  document.getElementById('ciHappy').textContent = cell[4] + '%';
  document.getElementById('ciHunger').textContent = cell[5] + '%';
  document.getElementById('ciRelig').textContent = cell[6] + '%';
  document.getElementById('ciEdu').textContent = cell[7] + '%';
  document.getElementById('ciWealth').textContent = cell[8];
}

function drawMap() {
  if (!state) return;
  const w = state.width, h = state.height;
  const cw = canvas.width / w, ch = canvas.height / h;
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const c = state.cells[y * w + x];
      const biome = c[0], owner = c[1], settle = c[2], pop = c[3], relig = c[6];

      let fill = BIOME_COLORS[biome];
      if (currentView === 'VISTA_POLITICA' && owner !== -1) {
        fill = state.countries[owner].color;
      } else if (currentView === 'VISTA_POBLACION' && biome !== 0) {
        const intensity = Math.min(1, pop / 8000);
        fill = `rgb(${Math.floor(30 + 220 * intensity)}, ${Math.floor(40 + 180 * intensity)}, 80)`;
      } else if (currentView === 'VISTA_RELIGION' && biome !== 0) {
        const rInt = relig / 100;
        fill = `rgb(${Math.floor(60 + 180 * rInt)}, 50, ${Math.floor(90 + 160 * rInt)})`;
      }

      ctx.fillStyle = fill;
      ctx.fillRect(x * cw, y * ch, cw + 0.5, ch + 0.5);

      if (settle === 3) {
        ctx.fillStyle = '#ffffff';
        ctx.beginPath();
        ctx.arc((x + 0.5) * cw, (y + 0.5) * ch, cw * 0.35, 0, Math.PI * 2);
        ctx.fill();
      } else if (settle === 2) {
        ctx.fillStyle = '#fde047';
        ctx.fillRect((x + 0.25) * cw, (y + 0.25) * ch, cw * 0.5, ch * 0.5);
      }
    }
  }

  if (hoverX >= 0 && hoverY >= 0 && currentTool !== 'inspect') {
    const rad = parseInt(document.getElementById('radiusInput').value || 3, 10);
    ctx.strokeStyle = currentTool === 'cataclysm' ? '#ef4444' : '#38bdf8';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc((hoverX + 0.5) * cw, (hoverY + 0.5) * ch, rad * cw, 0, Math.PI * 2);
    ctx.stroke();
  }
}

canvas.addEventListener('mousemove', e => {
  if (!state) return;
  const rect = canvas.getBoundingClientRect();
  hoverX = Math.floor((e.clientX - rect.left) / (canvas.width / state.width));
  hoverY = Math.floor((e.clientY - rect.top) / (canvas.height / state.height));
  drawMap();
});

canvas.addEventListener('click', () => {
  if (!state || hoverX < 0 || hoverY < 0) return;
  selectedX = hoverX;
  selectedY = hoverY;
  if (currentTool === 'inspect') {
    updateInspector(selectedX, selectedY);
  } else {
    const rad = parseInt(document.getElementById('radiusInput').value || 3, 10);
    apiCall(`/api/action?cmd=divine&type=${currentTool}&x=${hoverX}&y=${hoverY}&r=${rad}`);
  }
});

apiCall('/api/state');
</script>
</body>
</html>
)HTML";

// ============================================================================
// 4. SERVIDOR HTTP LIGERO EN SOCKETS NATIVOS (C++ PURO)
// ============================================================================

std::string getQueryParam(const std::string& query, const std::string& key) {
    std::string pattern = key + "=";
    size_t pos = query.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.length();
    size_t end = query.find_first_of("& ", pos);
    return query.substr(pos, end - pos);
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // Inicializamos un mapa de 40x40 celdas (1,600 celdas en std::vector)
    GodGameEngine engine(40, 40, 20261105);

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "Error al crear el socket del servidor grafico.\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(serverFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Error al enlazar el puerto 8080. Verifique que no haya otra instancia en ejecucion.\n";
        return 1;
    }

    listen(serverFd, 10);

    std::cout << "====================================================================\n";
    std::cout << " MOTOR C++ ESTRUCTURAS DE DATOS INICIADO CON INTERFAZ GRAFICA (GUI)\n";
    std::cout << " Abre en tu navegador: http://localhost:8080\n";
    std::cout << " (En GitHub Codespaces, haz clic en el boton 'Open in Browser')\n";
    std::cout << "====================================================================\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) continue;

        char buffer[4096] = {0};
        int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            std::string request(buffer, bytesRead);
            std::string firstLine = request.substr(0, request.find("\r\n"));

            std::string responseBody;
            std::string contentType = "application/json; charset=utf-8";

            if (firstLine.find("GET /api/action") != std::string::npos) {
                std::string cmd = getQueryParam(firstLine, "cmd");
                if (cmd == "tick") {
                    engine.advanceTick();
                } else if (cmd == "undo") {
                    engine.undoTick();
                } else if (cmd == "reset") {
                    unsigned int newSeed = 20261105;
                    try { newSeed = static_cast<unsigned int>(std::stoul(getQueryParam(firstLine, "seed"))); } catch (...) {}
                    engine.resetWorld(newSeed);
                } else if (cmd == "divine") {
                    std::string type = getQueryParam(firstLine, "type");
                    int x = std::stoi(getQueryParam(firstLine, "x"));
                    int y = std::stoi(getQueryParam(firstLine, "y"));
                    int r = std::stoi(getQueryParam(firstLine, "r"));
                    engine.applyDivineAction(type, x, y, r);
                } else if (cmd == "view") {
                    engine.pushUiView(getQueryParam(firstLine, "name"));
                }
                responseBody = engine.serializeStateToJson();
            } else if (firstLine.find("GET /api/state") != std::string::npos) {
                responseBody = engine.serializeStateToJson();
            } else {
                contentType = "text/html; charset=utf-8";
                responseBody = EMBEDDED_GUI_HTML;
            }

            std::ostringstream httpHeader;
            httpHeader << "HTTP/1.1 200 OK\r\n"
                       << "Content-Type: " << contentType << "\r\n"
                       << "Content-Length: " << responseBody.size() << "\r\n"
                       << "Cache-Control: no-store\r\n"
                       << "Connection: close\r\n\r\n";

            std::string fullResponse = httpHeader.str() + responseBody;
            send(clientFd, fullResponse.c_str(), fullResponse.size(), 0);
        }

#ifdef _WIN32
        closesocket(clientFd);
#else
        close(clientFd);
#endif
    }

    return 0;
}
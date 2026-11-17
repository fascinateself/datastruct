/**
 * PROYECTO DE ESTRUCTURAS DE DATOS - ENTREGA PARCIAL 1
 * Simulador de Civilizaciones (God Game Sandbox)
 * 
 * Estructuras de Datos Lineales Integradas:
 * 1. Arreglos Dinámicos (std::vector): Mapa 2D aplanado (O(1) acceso), registro de países e historial.
 * 2. Cola FIFO (std::queue): Procesamiento secuencial de acciones rutinarias por Tick (O(1)).
 * 3. Cola de Prioridad (std::priority_queue): Resolución de conflictos y eventos críticos (O(log N)).
 * 4. Pila LIFO (std::stack): Navegación de menús de interfaz y sistema de Undo lineal (O(1)).
 * 
 * Declaración de uso de IA: Estructura base, comentarios de complejidad asintótica y plantilla
 * de simulación generados con asistencia de IA (aprox. 45%), revisados y adaptados a los 
 * requerimientos del syllabus de Estructuras de Datos.
 */

#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <string>
#include <random>
#include <iomanip>
#include <cmath>
#include <algorithm>

// ============================================================================
// 1. DEFINICIÓN DE ENUMERACIONES Y CONSTANTES DEL MUNDO
// ============================================================================

enum class Biome { OCEAN, PLAINS, FOREST, MOUNTAIN, DESERT, TUNDRA };
enum class SettlementType { NONE, VILLAGE, CITY, CAPITAL };
enum class Era { 
    PRIMITIVE = 0,      // Era Primitiva: Fuego, Piedra, Domesticación, Mito
    ANCIENT = 1,        // Era Antigua: Escritura, Ciudad, Leyes, Metalurgia
    CLASSICAL = 2,      // Edad Clásica: Comercio, Cisma, Arquitectura, Herbolaria
    INDUSTRIAL = 3,     // Era Industrial: Vapor, Éxodo Rural, Método Científico
    TECHNOLOGICAL = 4   // Era Tecnológica: Red Global, Genética, Energía Limpia
};

enum class EventType {
    DIVINE_WRATH = 100,       // Prioridad máxima: Desastre o milagro directo de Dios
    PROPHET_APPEARANCE = 85,  // Aparición de un "Iluminado"
    EPIDEMIC_OUTBREAK = 70,   // Brote de enfermedad en celdas densas
    COLONIZATION_CLAIM = 50,  // Intento de expansión territorial (resuelve colisiones)
    TECH_BREAKTHROUGH = 30    // Descubrimiento de hito civilizatorio
};

enum class MenuScreen { MAIN_MAP, CELL_INSPECTOR, DIVINE_POWERS, WORLD_STATS };

// ============================================================================
// 2. TIPOS DE DATOS ABSTRACTOS (TDA) - ENTIDADES DEL SIMULADOR
// ============================================================================

struct Cell {
    int x = 0, y = 0;
    
    // Características inmutables
    Biome biome = Biome::OCEAN;
    float elevation = 0.0f;
    
    // Características mutables y políticas
    int ownerCountryId = -1; // -1 = Deshabitada / Sin país
    SettlementType settlement = SettlementType::NONE;
    
    // Demografía y sociedad
    long population = 0;
    float birthRate = 0.025f;
    float mortalityRate = 0.015f;
    float religiosity = 0.30f;
    float happiness = 0.70f;
    float hunger = 0.10f;
    float education = 0.05f;
    float wealth = 10.0f;
    bool hasReligion = false;

    bool isHabitable() const {
        return biome != Biome::OCEAN;
    }
};

struct Country {
    int id;
    std::string name;
    char symbol;
    Era currentEra = Era::PRIMITIVE;
    float techProgress = 0.0f;
    int controlledCells = 0;
    bool active = true;
    std::vector<std::string> unlockedAchievements; // Arreglo dinámico de logros del país
};

// Estructura para la Cola FIFO (std::queue) - Acciones rutinarias de cada Tick
struct RoutineTask {
    int cellIndex;
    std::string description;
};

// Estructura para la Cola de Prioridad (std::priority_queue) - Eventos con peso
struct PriorityEvent {
    EventType type;
    int priorityWeight;
    int targetX;
    int targetY;
    int countryId;
    std::string detail;

    // Sobrecarga del operador '<' para que std::priority_queue funcione como Max-Heap.
    // Complejidad de comparación: O(1).
    bool operator<(const PriorityEvent& other) const {
        if (priorityWeight != other.priorityWeight) {
            return priorityWeight < other.priorityWeight;
        }
        // Desempate determinista por coordenadas y país
        return countryId > other.countryId;
    }
};

// Instantánea del mundo para el sistema Undo en la Entrega 1 (vía Pila)
// Nota: En la Entrega Final, esto migrará a un Árbol N-ario para múltiples ramas temporales.
struct WorldSnapshot {
    int tickNumber;
    std::vector<Cell> gridState;
    std::vector<Country> countriesState;
    std::vector<std::string> eventLogState;
};

// ============================================================================
// 3. MOTOR PRINCIPAL DEL SIMULADOR (GESTIÓN DE ESTRUCTURAS LINEALES)
// ============================================================================

class GodGameEngine {
private:
    int width;
    int height;
    unsigned int worldSeed;
    int currentTick;
    std::mt19937 rng;

    // --- ESTRUCTURA LINEAL 1: ARREGLOS DINÁMICOS (std::vector) ---
    // Almacena el tablero en 1D para garantizar contigüidad en memoria y acceso O(1).
    std::vector<Cell> grid;
    std::vector<Country> countries;
    std::vector<std::string> globalHistoryLog;

    // --- ESTRUCTURA LINEAL 2: COLA ESTÁNDAR FIFO (std::queue) ---
    // Procesa la actualización demográfica y económica celda por celda en orden O(1).
    std::queue<RoutineTask> tickRoutineQueue;

    // --- ESTRUCTURA LINEAL 3: COLA DE PRIORIDAD (std::priority_queue) ---
    // Resuelve conflictos de colonización, plagas e intervenciones divinas en O(log K).
    std::priority_queue<PriorityEvent> eventResolutionQueue;

    // --- ESTRUCTURA LINEAL 4: PILAS LIFO (std::stack) ---
    // Gestiona la navegación entre pantallas de la UI y el historial de estados (Undo).
    std::stack<MenuScreen> uiNavigationStack;
    std::stack<WorldSnapshot> undoHistoryStack;

public:
    GodGameEngine(int w, int h, unsigned int seed) 
        : width(w), height(h), worldSeed(seed), currentTick(0), rng(seed) {
        
        // Inicializar la pila de interfaz en el mapa principal
        uiNavigationStack.push(MenuScreen::MAIN_MAP);
        generateProceduralWorld();
        spawnInitialCivilizations();
        saveSnapshot(); // Guardar estado en Tick 0
    }

    // Acceso en O(1) al arreglo dinámico aplanado
    inline int getIndex(int x, int y) const {
        return y * width + x;
    }

    inline bool isValidCoord(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    // Generación procedimental determinista basada en la semilla
    void generateProceduralWorld() {
        grid.resize(width * height);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Cell& c = grid[getIndex(x, y)];
                c.x = x;
                c.y = y;

                // Función trigonométrica determinista combinada con semilla para simular ruido
                float nx = static_cast<float>(x) / width - 0.5f;
                float ny = static_cast<float>(y) / height - 0.5f;
                float distFromCenter = std::sqrt(nx * nx + ny * ny) * 1.8f;
                float noise = (std::sin(x * 0.5f + worldSeed) + std::cos(y * 0.5f + worldSeed)) * 0.25f + 0.5f;
                
                c.elevation = noise - distFromCenter * 0.55f + (dist(rng) * 0.15f);

                if (c.elevation < 0.15f) {
                    c.biome = Biome::OCEAN;
                } else if (c.elevation < 0.25f) {
                    c.biome = Biome::DESERT;
                } else if (c.elevation < 0.55f) {
                    c.biome = Biome::PLAINS;
                } else if (c.elevation < 0.72f) {
                    c.biome = Biome::FOREST;
                } else {
                    c.biome = Biome::MOUNTAIN;
                }
            }
        }
        globalHistoryLog.push_back("[Tick 0] Mundo generado con semilla " + std::to_string(worldSeed));
    }

    void spawnInitialCivilizations() {
        std::vector<std::pair<std::string, char>> civTemplates = {
            {"Imperio Aethel", 'A'},
            {"Republica Boreal", 'B'},
            {"Dinastia Solaris", 'S'},
            {"Confederacion Valka", 'V'}
        };

        std::uniform_int_distribution<int> distX(1, width - 2);
        std::uniform_int_distribution<int> distY(1, height - 2);

        for (size_t i = 0; i < civTemplates.size(); ++i) {
            for (int attempts = 0; attempts < 100; ++attempts) {
                int rx = distX(rng);
                int ry = distY(rng);
                Cell& target = grid[getIndex(rx, ry)];

                if (target.isHabitable() && target.ownerCountryId == -1) {
                    Country newCountry;
                    newCountry.id = static_cast<int>(i);
                    newCountry.name = civTemplates[i].first;
                    newCountry.symbol = civTemplates[i].second;
                    newCountry.controlledCells = 1;
                    newCountry.unlockedAchievements.push_back("El Primer Fuego");
                    countries.push_back(newCountry);

                    target.ownerCountryId = newCountry.id;
                    target.settlement = SettlementType::CAPITAL;
                    target.population = 1200;
                    target.wealth = 50.0f;

                    globalHistoryLog.push_back("[Tick 0] Nace " + newCountry.name + " en (" + 
                                               std::to_string(rx) + "," + std::to_string(ry) + ").");
                    break;
                }
            }
        }
    }

    // Guarda el estado actual en la Pila de Historial (Complejidad O(N*M) por copia de vector)
    void saveSnapshot() {
        WorldSnapshot snap;
        snap.tickNumber = currentTick;
        snap.gridState = grid;
        snap.countriesState = countries;
        snap.eventLogState = globalHistoryLog;
        undoHistoryStack.push(snap);
    }

    // Deshace el último turno extrayendo el tope de la Pila LIFO en O(1)
    bool undoLastAction() {
        if (undoHistoryStack.size() <= 1) {
            return false; // Ya estamos en el estado raíz (Tick 0)
        }
        undoHistoryStack.pop(); // Descartar estado actual
        const WorldSnapshot& previous = undoHistoryStack.top();
        currentTick = previous.tickNumber;
        grid = previous.gridState;
        countries = previous.countriesState;
        globalHistoryLog = previous.eventLogState;
        return true;
    }

    // ========================================================================
    // CICLO DE SIMULACIÓN POR TICK (USO DE COLA FIFO Y COLA DE PRIORIDAD)
    // ========================================================================
    void advanceTick() {
        saveSnapshot();
        currentTick++;

        // FASE 1: Encolar todas las celdas habitadas en la Cola FIFO (std::queue)
        for (size_t idx = 0; idx < grid.size(); ++idx) {
            if (grid[idx].ownerCountryId != -1 && grid[idx].population > 0) {
                tickRoutineQueue.push({static_cast<int>(idx), "Actualizacion demografica y rolls"});
            }
        }

        std::uniform_real_distribution<float> probDist(0.0f, 1.0f);

        // FASE 2: Procesar la Cola FIFO secuencialmente (O(1) por desencolado)
        while (!tickRoutineQueue.empty()) {
            RoutineTask task = tickRoutineQueue.front();
            tickRoutineQueue.pop();

            Cell& cell = grid[task.cellIndex];
            Country& nation = countries[cell.ownerCountryId];

            // Crecimiento demográfico basado en natalidad, mortalidad y hambre
            float netGrowthRate = cell.birthRate - cell.mortalityRate - (cell.hunger * 0.02f);
            long deltaPop = static_cast<long>(cell.population * netGrowthRate);
            cell.population = std::max(0L, cell.population + deltaPop);

            // Evolución de asentamientos según población
            if (cell.population > 5000 && cell.settlement == SettlementType::VILLAGE) {
                cell.settlement = SettlementType::CITY;
                cell.education += 0.10f;
            } else if (cell.population > 800 && cell.settlement == SettlementType::NONE) {
                cell.settlement = SettlementType::VILLAGE;
            }

            // Roll de Avance Tecnológico
            float techChance = 0.15f + (cell.education * 0.3f);
            if (probDist(rng) < techChance) {
                nation.techProgress += 8.5f;
                if (nation.techProgress >= 100.0f && nation.currentEra != Era::TECHNOLOGICAL) {
                    eventResolutionQueue.push({
                        EventType::TECH_BREAKTHROUGH,
                        static_cast<int>(EventType::TECH_BREAKTHROUGH),
                        cell.x, cell.y, nation.id,
                        nation.name + " alcanza una nueva Era civilizatoria."
                    });
                }
            }

            // Roll de Epidemia si hay alta densidad y baja tecnología
            if (cell.population > 6000 && probDist(rng) < 0.04f) {
                eventResolutionQueue.push({
                    EventType::EPIDEMIC_OUTBREAK,
                    static_cast<int>(EventType::EPIDEMIC_OUTBREAK),
                    cell.x, cell.y, nation.id,
                    "Brote epidemico en (" + std::to_string(cell.x) + "," + std::to_string(cell.y) + ")"
                });
            }

            // Roll de Expansión / Colonización de celdas adyacentes
            if (cell.population > 1000 && probDist(rng) < 0.28f) {
                const int dx[4] = {0, 0, 1, -1};
                const int dy[4] = {1, -1, 0, 0};
                int dir = static_cast<int>(probDist(rng) * 4) % 4;
                int nx = cell.x + dx[dir];
                int ny = cell.y + dy[dir];

                if (isValidCoord(nx, ny)) {
                    Cell& neighbor = grid[getIndex(nx, ny)];
                    if (neighbor.isHabitable() && neighbor.ownerCountryId == -1) {
                        // La prioridad de colonización depende de la riqueza y población de la celda origen
                        int claimPriority = static_cast<int>(EventType::COLONIZATION_CLAIM) + 
                                            static_cast<int>(cell.wealth / 10.0f);
                        eventResolutionQueue.push({
                            EventType::COLONIZATION_CLAIM,
                            claimPriority,
                            nx, ny, nation.id,
                            nation.name + " reclama territorio en (" + std::to_string(nx) + "," + std::to_string(ny) + ")"
                        });
                    }
                }
            }
        }

        // FASE 3: Resolver la Cola de Prioridad (std::priority_queue) en orden de importancia O(log K)
        while (!eventResolutionQueue.empty()) {
            PriorityEvent ev = eventResolutionQueue.top();
            eventResolutionQueue.pop();

            Cell& targetCell = grid[getIndex(ev.targetX, ev.targetY)];

            switch (ev.type) {
                case EventType::DIVINE_WRATH:
                case EventType::PROPHET_APPEARANCE: {
                    globalHistoryLog.push_back("[Tick " + std::to_string(currentTick) + " | DIVINO] " + ev.detail);
                    break;
                }
                case EventType::EPIDEMIC_OUTBREAK: {
                    long lost = static_cast<long>(targetCell.population * 0.30f);
                    targetCell.population -= lost;
                    targetCell.happiness = std::max(0.1f, targetCell.happiness - 0.20f);
                    globalHistoryLog.push_back("[Tick " + std::to_string(currentTick) + "] " + ev.detail + 
                                               " (-" + std::to_string(lost) + " hab.)");
                    break;
                }
                case EventType::COLONIZATION_CLAIM: {
                    // Si dos países intentaron colonizar la misma celda en el mismo Tick,
                    // el que tuvo mayor prioridad en el Max-Heap llega primero y la ocupa.
                    if (targetCell.ownerCountryId == -1) {
                        targetCell.ownerCountryId = ev.countryId;
                        targetCell.population = 350;
                        targetCell.settlement = SettlementType::VILLAGE;
                        countries[ev.countryId].controlledCells++;
                        globalHistoryLog.push_back("[Tick " + std::to_string(currentTick) + "] " + ev.detail);
                    }
                    break;
                }
                case EventType::TECH_BREAKTHROUGH: {
                    Country& c = countries[ev.countryId];
                    if (c.techProgress >= 100.0f && c.currentEra != Era::TECHNOLOGICAL) {
                        c.techProgress = 0.0f;
                        c.currentEra = static_cast<Era>(static_cast<int>(c.currentEra) + 1);
                        std::string milestone = getEraMilestoneName(c.currentEra);
                        c.unlockedAchievements.push_back(milestone);
                        globalHistoryLog.push_back("[Tick " + std::to_string(currentTick) + " | HITO] " + 
                                                   c.name + " descubre: " + milestone);
                    }
                    break;
                }
            }
        }
    }

    // ========================================================================
    // REGLAS DE DIOS (INTERVENCIONES DEL JUGADOR)
    // ========================================================================

    // Acción 1: Enviar un "Iluminado" a un país (probabilidad según religiosidad y felicidad)
    void sendProphetToCountry(int countryId) {
        if (countryId < 0 || countryId >= static_cast<int>(countries.size())) return;
        saveSnapshot();

        Country& c = countries[countryId];
        int convertedCells = 0;
        std::uniform_real_distribution<float> prob(0.0f, 1.0f);

        for (Cell& cell : grid) {
            if (cell.ownerCountryId == countryId) {
                float effectiveness = cell.religiosity + (1.0f - cell.happiness) * 0.5f;
                if (prob(rng) < effectiveness) {
                    cell.hasReligion = true;
                    cell.religiosity = std::min(1.0f, cell.religiosity + 0.25f);
                    cell.happiness = std::min(1.0f, cell.happiness + 0.15f);
                    convertedCells++;
                }
            }
        }

        eventResolutionQueue.push({
            EventType::PROPHET_APPEARANCE,
            static_cast<int>(EventType::PROPHET_APPEARANCE),
            0, 0, countryId,
            "Un Iluminado predico en " + c.name + ", convirtiendo " + std::to_string(convertedCells) + " regiones."
        });
    }

    // Acción 2: Intervención en área circular (Cataclismo o Bendición de fertilidad)
    void castAreaMiracleOrDisaster(int centerX, int centerY, int radius, bool isBlessing) {
        saveSnapshot();
        int affected = 0;

        for (int y = centerY - radius; y <= centerY + radius; ++y) {
            for (int x = centerX - radius; x <= centerX + radius; ++x) {
                if (isValidCoord(x, y)) {
                    int dx = x - centerX;
                    int dy = y - centerY;
                    if (dx * dx + dy * dy <= radius * radius) {
                        Cell& cell = grid[getIndex(x, y)];
                        if (cell.isHabitable()) {
                            affected++;
                            if (isBlessing) {
                                cell.hunger = 0.0f;
                                cell.happiness = std::min(1.0f, cell.happiness + 0.30f);
                                cell.wealth += 25.0f;
                                cell.population = static_cast<long>(cell.population * 1.20f);
                            } else {
                                cell.population = static_cast<long>(cell.population * 0.45f);
                                cell.hunger = std::min(1.0f, cell.hunger + 0.40f);
                                if (cell.population < 100 && cell.ownerCountryId != -1) {
                                    countries[cell.ownerCountryId].controlledCells--;
                                    cell.ownerCountryId = -1;
                                    cell.settlement = SettlementType::NONE;
                                }
                            }
                        }
                    }
                }
            }
        }

        std::string actionName = isBlessing ? "Lluvia de Abundancia" : "Cataclismo Meteorico";
        eventResolutionQueue.push({
            EventType::DIVINE_WRATH,
            static_cast<int>(EventType::DIVINE_WRATH),
            centerX, centerY, -1,
            "Dios ejecuto " + actionName + " en (" + std::to_string(centerX) + "," + 
            std::to_string(centerY) + ") radio " + std::to_string(radius) + " (" + std::to_string(affected) + " celdas)."
        });
    }

    // ========================================================================
    // GESTIÓN DE PILA DE INTERFAZ Y RENDERIZADO EN CONSOLA
    // ========================================================================

    void navigateToScreen(MenuScreen screen) {
        uiNavigationStack.push(screen);
    }

    void navigateBack() {
        if (uiNavigationStack.size() > 1) {
            uiNavigationStack.pop();
        }
    }

    MenuScreen getCurrentScreen() const {
        return uiNavigationStack.top();
    }

    std::string getEraName(Era e) const {
        switch (e) {
            case Era::PRIMITIVE: return "1. Era Primitiva";
            case Era::ANCIENT: return "2. Era Antigua";
            case Era::CLASSICAL: return "3. Edad Clasica y Feudal";
            case Era::INDUSTRIAL: return "4. Era Industrial";
            case Era::TECHNOLOGICAL: return "5. Era Tecnologica";
        }
        return "Desconocida";
    }

    std::string getEraMilestoneName(Era e) const {
        switch (e) {
            case Era::PRIMITIVE: return "Dominio de la Piedra";
            case Era::ANCIENT: return "Invencion de la Escritura y Codigo de Leyes";
            case Era::CLASSICAL: return "Rutas Comerciales y Arquitectura Monumental";
            case Era::INDUSTRIAL: return "Maquina de Vapor y Metodo Cientifico";
            case Era::TECHNOLOGICAL: return "Red de Comunicaciones Global y Energia Limpia";
        }
        return "Hito";
    }

    void renderDashboard() const {
        std::cout << "\n====================================================================\n";
        std::cout << "  GOD GAME SANDBOX | Semilla: " << worldSeed 
                  << " | Tick Actual: " << currentTick 
                  << " | Estados en Pila Undo: " << undoHistoryStack.size() << "\n";
        std::cout << "====================================================================\n";

        // Renderizar cuadrícula del mundo (Arreglo Dinámico)
        std::cout << "   ";
        for (int x = 0; x < width; ++x) std::cout << (x % 10) << " ";
        std::cout << "\n";

        for (int y = 0; y < height; ++y) {
            std::cout << std::setw(2) << y << " ";
            for (int x = 0; x < width; ++x) {
                const Cell& c = grid[getIndex(x, y)];
                if (c.ownerCountryId != -1) {
                    char sym = countries[c.ownerCountryId].symbol;
                    if (c.settlement == SettlementType::CAPITAL) {
                        std::cout << sym << "*";
                    } else {
                        std::cout << sym << " ";
                    }
                } else {
                    switch (c.biome) {
                        case Biome::OCEAN:    std::cout << "~ "; break;
                        case Biome::PLAINS:   std::cout << ". "; break;
                        case Biome::FOREST:   std::cout << "T "; break;
                        case Biome::MOUNTAIN: std::cout << "^ "; break;
                        case Biome::DESERT:   std::cout << ": "; break;
                        case Biome::TUNDRA:   std::cout << "- "; break;
                    }
                }
            }
            std::cout << "\n";
        }

        std::cout << "\n--- ESTADO DE LAS CIVILIZACIONES (std::vector<Country>) ---\n";
        for (const Country& c : countries) {
            std::cout << " [" << c.symbol << "] " << std::left << std::setw(20) << c.name
                      << " | Celdas: " << std::setw(3) << c.controlledCells
                      << " | " << std::setw(24) << getEraName(c.currentEra)
                      << " | Tech: " << std::fixed << std::setprecision(1) << c.techProgress << "%\n";
        }

        std::cout << "\n--- ULTIMOS EVENTOS PROCESADOS ---\n";
        int startIdx = std::max(0, static_cast<int>(globalHistoryLog.size()) - 5);
        for (size_t i = startIdx; i < globalHistoryLog.size(); ++i) {
            std::cout << "  > " << globalHistoryLog[i] << "\n";
        }
        std::cout << "====================================================================\n";
    }

    void inspectCell(int x, int y) const {
        if (!isValidCoord(x, y)) {
            std::cout << "Coordenadas fuera del rango del mapa.\n";
            return;
        }
        const Cell& c = grid[getIndex(x, y)];
        std::cout << "\n--- INSPECTOR DE CELDA (" << x << ", " << y << ") ---\n";
        std::cout << "  Habitable: " << (c.isHabitable() ? "Si" : "No (Oceano)") << "\n";
        std::cout << "  Dueno: " << (c.ownerCountryId != -1 ? countries[c.ownerCountryId].name : "Ninguno") << "\n";
        std::cout << "  Poblacion: " << c.population << " habitantes\n";
        std::cout << "  Felicidad: " << (c.happiness * 100.0f) << "% | Hambre: " << (c.hunger * 100.0f) << "%\n";
        std::cout << "  Religiosidad: " << (c.religiosity * 100.0f) << "% | Educacion: " << (c.education * 100.0f) << "%\n";
    }
};

// ============================================================================
// 4. PUNTO DE ENTRADA Y DEMOSTRACIÓN AUTOMÁTICA / INTERACTIVA
// ============================================================================

int main() {
    // Mapa de 20x12 para visualización limpia en consola (escalable a 100x100)
    GodGameEngine simulator(20, 12, 20261105);

    std::cout << "Iniciando simulacion demostrativa de la Entrega Parcial 1...\n";
    simulator.renderDashboard();

    // Avanzar 3 Ticks automáticamente para demostrar std::queue y std::priority_queue
    for (int t = 0; t < 3; ++t) {
        simulator.advanceTick();
    }

    // Intervención divina: Enviar un Iluminado al país 0 y lanzar Bendición en (10, 6)
    simulator.sendProphetToCountry(0);
    simulator.castAreaMiracleOrDisaster(10, 6, 4, true);
    simulator.advanceTick();

    simulator.renderDashboard();
    simulator.inspectCell(10, 6);

    // Demostración de std::stack para deshacer el último Tick (Undo)
    std::cout << "\n[DEMO UNDO] Ejecutando Undo mediante std::stack<WorldSnapshot>...\n";
    if (simulator.undoLastAction()) {
        std::cout << "Estado restaurado exitosamente al Tick anterior.\n";
        simulator.renderDashboard();
    }

    return 0;
}
// =======================
// Watch Market Tycoon core
// WebAssembly/iPhone safe version
// =======================

#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// --------------------
// Basic data for a watch listing
// --------------------
struct WatchItem {
    std::string brand;
    std::string model;
    std::string note;   // provenance / sketchy parts / story
    int condition;      // 1=trashed 10=mint
    int price;          // asking
    int yourCost;       // what you paid (0 if not bought)
    bool owned;
};

// --------------------
// Game state
// --------------------
class Game {
public:
    bool init();
    void shutdown();
    void run();
    void frameOnce();            // one frame of logic+draw
    void click(int x, int y);    // handle taps
    void refreshMarket();        // new watches on market
    void nextDay();              // advance time
    void save();
    void load();

    // draw helpers
    void drawUI();
    void drawMarket();
    void drawInventory();
    void drawMap();

    void step(); // update + draw, called every frame

private:
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;

    // game info
    int day = 1;
    int cash = 5000;
    int rep = 1;
    std::string location = "NYC Diamond District";

    // screens
    enum Screen {
        SCREEN_MARKET,
        SCREEN_INVENTORY,
        SCREEN_MAP
    } screen = SCREEN_MARKET;

    // lists
    std::vector<WatchItem> market;
    std::vector<WatchItem> inventory;

    // ui tap boxes
    SDL_Rect tabMarket;
    SDL_Rect tabInv;
    SDL_Rect tabMap;
    SDL_Rect actionNextDay;
    SDL_Rect actionRefresh;

    // simple helper
    void buyWatch(int idx);
};

// We need a global pointer so emscripten can call into our game every "tick"
static Game* G = nullptr;

// --------------------
// Utility random watch generator
// --------------------
static WatchItem randomWatch() {
    static const char* brands[] = {
        "Rolex", "Omega", "Tudor", "IWC", "Sinn", "Patek Philippe",
        "Doxa", "Damasko", "Christopher Ward"
    };
    static const char* models[] = {
        "Submariner 16610 (polished lugs)",
        "Speedmaster Pro (service dial)",
        "Black Bay 58 (no box)",
        "Mark XV (lume pip missing)",
        "U1 Tegiment (desk dive wear)",
        "Nautilus-ish Franken (vintage Rolex 1570 inside)",
        "SUB 300 Sharkhunter (needs service)",
        "DA36 (tool watch, tough)",
        "C60 Trident (pretty clean)"
    };
    static const char* notes[] = {
        "Papers lost. Seller says 'family piece'.",
        "Service parts. Mix of gen/vintage. Could flip fast.",
        "Dial has patina. Might be water damage or tropical.",
        "No bracelet, only NATO.",
        "Movement running +20s/day. Might need service.",
        "Caseback swapped. Sketchy.",
        "Mint crystal, scratched bezel.",
        "Lume mismatch.",
        "Full kit. Safe buy."
    };

    WatchItem w;
    w.brand = brands[rand() % (sizeof(brands)/sizeof(brands[0]))];
    w.model = models[rand() % (sizeof(models)/sizeof(models[0]))];
    w.note  = notes[rand() % (sizeof(notes)/sizeof(notes[0]))];
    w.condition = 6 + (rand() % 5);        // 6-10
    w.price = 800 + (rand() % 7500);       // asking
    w.yourCost = 0;
    w.owned = false;
    return w;
}

// --------------------
// Game methods
// --------------------

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init fail: " << SDL_GetError() << "\n";
        return false;
    }

    // fixed logical size for UI layout
    int w = 400;
    int h = 700;

    win = SDL_CreateWindow(
        "Watch Market Tycoon",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN
    );
    if (!win) {
        std::cerr << "CreateWindow fail: " << SDL_GetError() << "\n";
        return false;
    }

    // renderer with fallbacks so iPhone/WebGL doesn't choke
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren) {
        std::cerr << "CreateRenderer fail: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // set up clickable tab regions (bottom buttons)
    tabMarket = {  0, h-60, 130, 60 };
    tabInv    = {130, h-60, 140, 60 };
    tabMap    = {270, h-60, 130, 60 };

    // actions at top
    actionNextDay = { w-110, 40, 100, 28 };
    actionRefresh = { w-110, 72, 100, 28 };

    // seed rng
    srand((unsigned)time(NULL));

    // load save if possible
    load();

    if (market.empty()) {
        refreshMarket();
    }

    return true;
}

void Game::shutdown() {
    save();
    if (ren) { SDL_DestroyRenderer(ren); ren = nullptr; }
    if (win) { SDL_DestroyWindow(win);   win = nullptr; }
    SDL_Quit();
}

void Game::refreshMarket() {
    market.clear();
    for (int i = 0; i < 6; i++) {
        market.push_back(randomWatch());
    }
}

void Game::nextDay() {
    day += 1;
    // maybe raise rep if you hold nice pieces
    if (!inventory.empty() && rep < 10) {
        rep += 1;
    }
    // market rotates
    refreshMarket();
}

void Game::save() {
    std::ofstream f("save.txt");
    if (!f.good()) return;
    f << day << " " << cash << " " << rep << "\n";
    f << location << "\n";
    f << inventory.size() << "\n";
    for (auto &w : inventory) {
        f << w.brand << "\n"
          << w.model << "\n"
          << w.note  << "\n"
          << w.condition << " "
          << w.price << " "
          << w.yourCost << " "
          << w.owned << "\n";
    }
}

void Game::load() {
    std::ifstream f("save.txt");
    if (!f.good()) return;
    size_t count=0;
    f >> day >> cash >> rep;
    f.ignore();
    std::getline(f, location);
    f >> count;
    f.ignore();
    inventory.clear();
    for (size_t i=0;i<count;i++) {
        WatchItem w;
        std::getline(f, w.brand);
        std::getline(f, w.model);
        std::getline(f, w.note);
        f >> w.condition >> w.price >> w.yourCost >> w.owned;
        f.ignore();
        inventory.push_back(w);
    }
}

// buy from market taps
void Game::buyWatch(int idx) {
    if (idx < 0 || idx >= (int)market.size()) return;
    WatchItem &w = market[idx];
    if (w.owned) return;
    if (cash >= w.price) {
        cash -= w.price;
        w.owned = true;
        w.yourCost = w.price;
        // move to inventory
        inventory.push_back(w);
    }
}

// very lo-fi UI rectangles and text-like blocks
static void drawRect(SDL_Renderer* r, int x,int y,int w,int h,
                     Uint8 R,Uint8 Gc,Uint8 B,Uint8 A)
{
    SDL_Rect box{ x,y,w,h };
    SDL_SetRenderDrawColor(r,R,Gc,B,A);
    SDL_RenderFillRect(r,&box);
}

static void drawLabel(SDL_Renderer* r, int x,int y,
                      const std::string &text,
                      Uint8 R,Uint8 Gc,Uint8 B,Uint8 A)
{
    // draw a simple stub box for text (you'll replace with bitmap font later)
    drawRect(r,x,y, (int)text.size()*6,10, R,Gc,B,A);
}

// top HUD: money, rep, day, location, buttons
void Game::drawUI() {
    // HUD background bar
    drawRect(ren,0,0,400,110,20,24,32,255);

    // pseudo-text
    drawLabel(ren,10,10,"Bankroll: $" + std::to_string(cash),255,255,255,255);
    drawLabel(ren,10,26,"Rep: " + std::to_string(rep),200,200,255,255);
    drawLabel(ren,10,42,"Day: " + std::to_string(day),200,255,200,255);
    drawLabel(ren,10,58,"Loc: " + location,255,200,200,255);

    // next day button
    drawRect(ren, actionNextDay.x, actionNextDay.y,
             actionNextDay.w, actionNextDay.h,
             40,80,40,255);
    drawLabel(ren, actionNextDay.x+6, actionNextDay.y+8,
              "Next Day",0,0,0,255);

    // refresh button
    drawRect(ren, actionRefresh.x, actionRefresh.y,
             actionRefresh.w, actionRefresh.h,
             80,40,40,255);
    drawLabel(ren, actionRefresh.x+6, actionRefresh.y+8,
              "Refresh",0,0,0,255);
}

void Game::drawMarket() {
    // market panel
    drawRect(ren,0,110,400,530,18,18,24,255);

    int y = 120;
    for (int i=0;i<(int)market.size();i++) {
        WatchItem &w = market[i];
        // card bg
        drawRect(ren,10,y,380,70,30,30,40,255);

        // brand/model blocks
        drawLabel(ren,20,y+10,w.brand + " " + w.model,255,255,255,255);

        // price
        drawLabel(ren,20,y+26,"Ask $" + std::to_string(w.price),200,255,200,255);

        // note
        drawLabel(ren,20,y+42,w.note,180,180,255,255);

        // tap zone is basically that rect
        y += 80;
    }
}

void Game::drawInventory() {
    drawRect(ren,0,110,400,530,24,18,18,255);

    int y = 120;
    for (int i=0;i<(int)inventory.size();i++) {
        WatchItem &w = inventory[i];
        drawRect(ren,10,y,380,70,40,30,30,255);

        drawLabel(ren,20,y+10,w.brand + " " + w.model,255,255,255,255);
        drawLabel(ren,20,y+26,"Paid $" + std::to_string(w.yourCost),255,255,200,255);
        drawLabel(ren,20,y+42,"Cond " + std::to_string(w.condition) + "/10",200,200,255,255);

        y += 80;
    }
}

void Game::drawMap() {
    drawRect(ren,0,110,400,530,18,24,18,255);

    // show location choices
    drawRect(ren,20,140,360,50,60,60,90,255);
    drawLabel(ren,30,150,"NYC Diamond District",255,255,255,255);

    drawRect(ren,20,210,360,50,60,90,60,255);
    drawLabel(ren,30,220,"Paris Vintage Arcade",255,255,255,255);

    drawRect(ren,20,280,360,50,90,60,60,255);
    drawLabel(ren,30,290,"London Hatton Garden",255,255,255,255);

    drawRect(ren,20,350,360,50,90,90,60,255);
    drawLabel(ren,30,360,"Flea / Telegram / Forums",255,255,255,255);
}

// bottom nav bar
static void drawTab(SDL_Renderer* r, SDL_Rect rc,
                    const std::string &label,
                    bool active)
{
    if (active) {
        drawRect(r,rc.x,rc.y,rc.w,rc.h,60,60,100,255);
    } else {
        drawRect(r,rc.x,rc.y,rc.w,rc.h,30,30,40,255);
    }
    drawLabel(r,rc.x+8,rc.y+25,label,255,255,255,255);
}

void Game::step() {
    // clear bg frame
    SDL_SetRenderDrawColor(ren, 11,15,25,255);
    SDL_RenderClear(ren);

    // HUD
    drawUI();

    // screen content
    switch (screen) {
        case SCREEN_MARKET:    drawMarket(); break;
        case SCREEN_INVENTORY: drawInventory(); break;
        case SCREEN_MAP:       drawMap(); break;
    }

    // bottom tabs
    drawTab(ren, tabMarket, "Market",    screen==SCREEN_MARKET);
    drawTab(ren, tabInv,    "Inventory", screen==SCREEN_INVENTORY);
    drawTab(ren, tabMap,    "Map",       screen==SCREEN_MAP);

    SDL_RenderPresent(ren);
}

void Game::click(int x, int y) {
    // tabs at bottom
    if (SDL_PointInRect(&SDL_Point{ x,y }, &tabMarket)) {
        screen = SCREEN_MARKET;
        return;
    }
    if (SDL_PointInRect(&SDL_Point{ x,y }, &tabInv)) {
        screen = SCREEN_INVENTORY;
        return;
    }
    if (SDL_PointInRect(&SDL_Point{ x,y }, &tabMap)) {
        screen = SCREEN_MAP;
        return;
    }

    // top buttons
    if (SDL_PointInRect(&SDL_Point{ x,y }, &actionNextDay)) {
        nextDay();
        save();
        return;
    }
    if (SDL_PointInRect(&SDL_Point{ x,y }, &actionRefresh)) {
        refreshMarket();
        save();
        return;
    }

    // market buy taps:
    if (screen == SCREEN_MARKET) {
        // cards start y=120, height=70 + 10 margin
        int idx = (y - 120) / 80;
        if (idx >=0 && idx < (int)market.size()) {
            buyWatch(idx);
        }
    }

    // change location in map:
    if (screen == SCREEN_MAP) {
        if (y>140 && y<190) location = "NYC Diamond District";
        else if (y>210 && y<260) location = "Paris Vintage Arcade";
        else if (y>280 && y<330) location = "London Hatton Garden";
        else if (y>350 && y<400) location = "Online / Telegram / Forums";
        save();
    }
}

// --------------------
// Web loop integration
// --------------------

// The function emscripten calls every frame:
static void em_frame(void*) {
    // handle input / events
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            if (G) { G->save(); G->shutdown(); }
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
            return;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            if (G) G->click(e.button.x, e.button.y);
        }
        if (e.type == SDL_KEYDOWN) {
            if (!G) break;
            if (e.key.keysym.sym == SDLK_r) { G->refreshMarket(); G->save(); }
            if (e.key.keysym.sym == SDLK_n) { G->nextDay(); G->save(); }
            if (e.key.keysym.sym == SDLK_s) { G->save(); }
            if (e.key.keysym.sym == SDLK_l) { G->load(); }
        }
    }

    // one frame of game
    if (G) G->step();
}

void Game::run() {
#ifdef __EMSCRIPTEN__
    // Browser / iPhone path:
    emscripten_set_main_loop_arg(em_frame, nullptr, 0, 1);
#else
    // Desktop fallback path:
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                click(e.button.x, e.button.y);
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_r) { refreshMarket(); save(); }
                if (e.key.keysym.sym == SDLK_n) { nextDay(); save(); }
                if (e.key.keysym.sym == SDLK_s) { save(); }
                if (e.key.keysym.sym == SDLK_l) { load(); }
            }
        }

        step();
        SDL_Delay(16);
    }
    save();
    shutdown();
#endif
}

// --------------------
// main()
// --------------------

int main(int argc, char** argv) {
    static Game game;
    G = &game;
    if (!game.init()) {
        std::cerr << "Init failed\n";
        return 1;
    }
    game.run();
    return 0;
}

#include <SDL.h>
#include <SDL_image.h>
#include <string>
#include <vector>
#include <random>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
EM_JS(void, js_save, (const char* s), {
  try { localStorage.setItem('wm_save', UTF8ToString(s)); } catch(e) {}
});
EM_JS(int, js_load, (char* out, int maxlen), {
  try {
    var s = localStorage.getItem('wm_save');
    if(!s) return 0;
    var len = lengthBytesUTF8(s)+1;
    if(len > maxlen) len = maxlen;
    stringToUTF8(s, out, len);
    return 1;
  } catch(e) { return 0; }
});
#else
void js_save(const char* s){ std::ofstream f("save.txt"); if(f){ f<<s; } }
int js_load(char* out, int maxlen){
  std::ifstream f("save.txt"); if(!f) return 0;
  std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if((int)s.size()+1 > maxlen) s.resize(maxlen-1);
  std::copy(s.begin(), s.end(), out); out[s.size()] = '\0'; return 1;
}
#endif

static std::mt19937 rng{std::random_device{}()};
int irand(int a, int b){ std::uniform_int_distribution<int> d(a,b); return d(rng); }
bool chance(int pct){ std::uniform_int_distribution<int> d(1,100); return d(rng)<=pct; }
float frand(float a, float b){ std::uniform_real_distribution<float> d(a,b); return d(rng); }
std::string money(int n){ std::stringstream ss; ss << "$" << n; return ss.str(); }

enum class Scene { Market, Inventory, Auctions, Telegram, Parts, Map, Detail };

struct Item { std::string id, type, name, brand, ref, cond, risk, img; int ask=0, cost=0; };
struct Button { SDL_Rect r; std::string label; };

struct Game {
  int W=1280, H=720;
  SDL_Window* win=nullptr; SDL_Renderer* ren=nullptr;
  Scene scene = Scene::Market;
  int day=1, cash=25000, rep=0, lvl=1, xp=0, xpNext=100;
  std::string location = "NYC • Diamond District";
  std::vector<Item> market, inventory;
  int selected = -1;
  std::vector<Button> tabs;
  Button btnRefresh{{900,90,110,36},"Refresh"};
  Button btnNext{{1020,90,120,36},"Next Day"};
  Button btnPromote{{20,640,160,36},"Promote"};
  Button btnQuick{{190,640,170,36},"Quick Sell"};
  Button btnBreak{{370,640,180,36},"Break to Parts"};
  Button btnBack{{20,90,90,36},"< Back"};
  SDL_Color ink{233,241,255,255}, mut{159,176,195,255};
  SDL_Color panel{16,26,42,255}, line{38,58,96,255}, accent{121,192,255,255};
  SDL_Color ok{120,220,160,255}, warn{255,209,102,255};

  bool init(){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0) return false;
    win = SDL_CreateWindow("Watch Market Tycoon (Web)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
    if(!win) return false;
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(!ren) return false;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    int x=10; int y=90; const char* names[]={"Market","Inventory","Auctions","Telegram","Parts","Map"};
    for(int i=0;i<6;i++){ Button b; b.r={x,y,120,36}; b.label=names[i]; tabs.push_back(b); x+=126; }
    generateMarket(true); load(); return true;
  }

  std::string genId(){ static const char* al="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; std::string s; s.reserve(8); for(int i=0;i<8;i++) s.push_back(al[irand(0,35)]); return s; }
  Item makeWatch(){
    static std::vector<std::string> brands={"Rolex","Omega","Tudor","Sinn","IWC","Patek Philippe","Christopher Ward","Damasko","Doxa","Seiko","Longines","Oris","Breitling","TAG Heuer","Cartier","Hamilton"};
    std::string brand = chance(35)? "Rolex" : brands[irand(0,(int)brands.size()-1)];
    std::string ref="";
    if(brand=="Rolex"){ std::vector<std::string> r={"16610","14060M","16710"}; ref=r[irand(0,(int)r.size()-1)]; }
    else if(brand=="Sinn"){ std::vector<std::string> r={"U1","104","356"}; ref=r[irand(0,(int)r.size()-1)]; }
    std::string cond = std::vector<std::string>{"VG","Good","Fair","Serviced","Unserviced"}[irand(0,4)];
    int ask = brand=="Rolex"? irand(3500,16000) : brand=="Omega"? irand(1500,4500) : brand=="Tudor"? irand(1200,4000) : irand(200,3000);
    bool parts = (brand!="Rolex" && chance(15));
    std::string risk = chance(10)? "Aftermarket dial risk" : (parts? "Vintage Rolex parts swap": "");
    ask += parts? irand(200,1200):0;
    int cost = std::max(50,(int)std::floor(ask * frand(0.55f,0.8f)));
    std::string name = (brand=="Omega")? "Omega Speedmaster Pro" : (brand=="Sinn" && ref=="U1")? "Sinn U1" : (ref.empty()? brand+" auto" : brand+" "+ref);
    Item it; it.id=genId(); it.type="watch"; it.brand=brand; it.ref=ref; it.name=name; it.cond=cond; it.ask=ask; it.cost=cost; it.risk=risk; it.img=""; return it;
  }
  Item makePart(){
    static std::vector<std::string> brands={"Rolex","Omega","Tudor","Sinn","IWC","Patek Philippe","Christopher Ward","Damasko","Doxa","Seiko","Longines","Oris","Breitling","TAG Heuer","Cartier","Hamilton"};
    std::string brand = chance(45)? "Rolex" : brands[irand(0,(int)brands.size()-1)];
    std::string kind = std::vector<std::string>{"dial","hands","bezel","insert","bracelet","crown","crystal","movement","date wheel"}[irand(0,8)];
    std::string ref = (brand=="Rolex")? std::vector<std::string>{"16610","14060M","16710"}[irand(0,2)] : "";
    std::string cond = std::vector<std::string>{"NOS","VG","Fair","For parts"}[irand(0,3)];
    std::string mv = (kind=="movement")? std::vector<std::string>{"3135","ETA 2824-2","SW200-1","6R35"}[irand(0,3)] : "";
    int ask = (brand=="Rolex")? irand(180,3500) : irand(30,700);
    int cost = std::max(20,(int)std::floor(ask * frand(0.55f,0.8f)));
    std::string name = brand + (ref.empty()? " " : " " + ref + " ") + kind + (mv.empty()? "" : " ("+mv+")");
    std::string risk = (brand=="Rolex" && chance(25))? "Aftermarket risk" : "";
    Item it; it.id=genId(); it.type="part"; it.brand=brand; it.ref=ref; it.name=name; it.cond=cond; it.ask=ask; it.cost=cost; it.risk=risk; it.img=""; return it;
  }
  void generateMarket(bool first=false){
    market.clear(); int n = irand(7,11);
    for(int i=0;i<n;i++) market.push_back(chance(40)? makePart(): makeWatch());
  }
  void refreshMarket(){
    for(auto& it: market){
      float swing = (it.type=="watch")? 0.08f : 0.12f;
      float drift = 1.f + frand(-swing/2.f, swing/2.f);
      it.ask = std::max(20,(int)std::floor(it.ask * drift));
      if(chance(7)) it.risk = it.risk.empty()? "Provenance unclear":"";
      if(chance(6)) it.cond = std::vector<std::string>{"VG","Good","Fair","Serviced","Unserviced"}[irand(0,4)];
    }
    market.erase(std::remove_if(market.begin(),market.end(),[](const Item&){ return chance(10); }), market.end());
    int addN = irand(1,3); for(int i=0;i<addN;i++) market.push_back(chance(40)? makePart(): makeWatch());
  }
  void nextDay(){ day++; cash -= (int)inventory.size()*7; if(chance(30)) rep++; if(chance(10)&&rep>0) rep--; generateMarket(); }

  std::string encode(){
    std::stringstream s; s<<day<<","<<cash<<","<<rep<<"\n";
    for(auto& it: inventory){
      s<<"I,"<<it.type<<","<<it.brand<<","<<it.ref<<","<<it.cond<<","<<it.ask<<","<<it.cost<<","<<it.name<<"\n";
    }
    return s.str();
  }
  void parse(const std::string& data){
    std::stringstream ss(data); std::string line; inventory.clear();
    if(std::getline(ss,line)){ std::stringstream a(line); char c; a>>day>>c>>cash>>c>>rep; }
    while(std::getline(ss,line)){
      if(line.size()<2 || line[0]!='I') continue;
      std::stringstream b(line); std::string tok; Item it;
      std::getline(b,tok,','); // I
      std::getline(b,it.type,','); std::getline(b,it.brand,','); std::getline(b,it.ref,','); std::getline(b,it.cond,',');
      std::getline(b,tok,','); it.ask=std::stoi(tok); std::getline(b,tok,','); it.cost=std::stoi(tok); std::getline(b,it.name,'\n');
      it.id=genId(); it.risk=""; it.img=""; inventory.push_back(it);
    }
  }
  void save(){ auto s=encode(); js_save(s.c_str()); }
  void load(){ char buf[1<<16]; if(js_load(buf,sizeof(buf))){ parse(std::string(buf)); } }

  void fill(SDL_Rect r, SDL_Color c){ SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,c.a); SDL_RenderFillRect(ren,&r); }
  void border(SDL_Rect r, SDL_Color c){ SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,c.a); SDL_RenderDrawRect(ren,&r); }
  void text(int x,int y,const std::string& s, SDL_Color c){
    int px=x, py=y; for(char ch: s){ if(ch=='\n'){ py+=14; px=x; continue; } SDL_Rect dot{px,py,2,10}; SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,c.a); SDL_RenderFillRect(ren,&dot); px+=6; }
  }
  void header(){
    fill({0,0,W,80}, SDL_Color{10,14,20,230}); border({0,0,W,80}, SDL_Color{27,41,70,255});
    text(12,10,"WATCH MARKET TYCOON", ink);
    text(12,44,"Bankroll: "+money(cash)+"   Rep: "+std::to_string(rep)+"   Day "+std::to_string(day)+"   Loc: "+location, SDL_Color{190,210,240,255});
  }
  void navBar(){
    fill({0,80,W,50}, SDL_Color{16,24,41,230}); border({0,80,W,50}, SDL_Color{22,36,65,255});
    const char* names[]={"Market","Inventory","Auctions","Telegram","Parts","Map"};
    for(size_t i=0;i<tabs.size();++i){
      auto r = tabs[i].r; bool active = (int)scene==(int)i;
      fill(r, active? SDL_Color{30,55,100,255} : SDL_Color{28,38,64,255}); border(r, SDL_Color{50,70,110,255});
      text(r.x+8,r.y+10,names[i], SDL_Color{210,230,255,255});
    }
    drawBtn(btnRefresh); drawBtn(btnNext);
  }
  void drawBtn(const Button& b){ fill(b.r, SDL_Color{36,50,77,255}); border(b.r, SDL_Color{43,59,93,255}); text(b.r.x+8,b.r.y+10,b.label, SDL_Color{230,240,255,255}); }
  void drawCard(int x,int y,const Item& it, bool hovered){
    SDL_Rect card{x,y,760,100}; fill(card, hovered? SDL_Color{22,30,52,255}:SDL_Color{17,26,42,255}); border(card, line);
    fill({x+8,y+8,96,84}, SDL_Color{11,15,25,255}); border({x+8,y+8,96,84}, SDL_Color{42,57,90,255});
    text(x+116,y+10,it.name, SDL_Color{220,235,255,255});
    text(x+116,y+36, it.brand + (it.ref.size()? " ref "+it.ref:"") + " • " + it.cond + (it.risk.size()? " • "+it.risk:""), SDL_Color{160,185,220,255});
    text(x+116,y+66, "Ask: " + money(it.ask), ok); text(x+650,y+72, "Click", SDL_Color{150,170,210,255});
  }

  bool hit(const SDL_Rect& r, int mx,int my){ return mx>=r.x && mx<=r.x+r.w && my>=r.y && my<=r.y+r.h; }
  void click(int mx,int my){
    for(size_t i=0;i<tabs.size();++i) if(hit(tabs[i].r,mx,my)){ scene=(Scene)i; return; }
    if(hit(btnRefresh.r,mx,my)){ refreshMarket(); save(); return; }
    if(hit(btnNext.r,mx,my)){ nextDay(); save(); return; }
    switch(scene){
      case Scene::Market:{
        int x=20,y=150; for(size_t i=0;i<market.size();++i){ SDL_Rect r{x,y+(int)i*110,760,100}; if(hit(r,mx,my)){ selected=(int)i; scene=Scene::Detail; return; } }
      }break;
      case Scene::Inventory:{
        if(hit(btnPromote.r,mx,my)){ if(!inventory.empty()){ cash -= std::min(500,(int)inventory.size()*6); rep++; save(); } return; }
        if(hit(btnQuick.r,mx,my)){ int tot=0; for(auto& it: inventory) tot += (int)std::floor(it.ask*0.9); cash += tot; inventory.clear(); save(); return; }
        if(hit(btnBreak.r,mx,my)){
          int idx=-1; for(size_t i=0;i<inventory.size();++i) if(inventory[i].type=="watch"){ idx=(int)i; break; }
          if(idx>=0){ Item base=inventory[idx]; inventory.erase(inventory.begin()+idx);
            std::vector<std::string> parts={"dial","hands","bezel/insert","bracelet","crown"};
            for(auto& k:parts){ Item p; p.id=genId(); p.type="part"; p.brand=base.brand; p.ref=base.ref; p.name=base.brand+" "+(base.ref.empty()? "": base.ref+" ")+k; p.cond="Fair"; p.ask=(int)std::floor(base.ask*0.18f); p.cost=0; inventory.push_back(p); }
            save();
          } return;
        }
        { int x=20,y=180;
          for(size_t i=0;i<inventory.size();++i){ SDL_Rect r{x,y+(int)i*110,760,100}; if(hit(r,mx,my)){ cash += inventory[i].ask; inventory.erase(inventory.begin()+i); save(); return; } }
        }
      }break;
      case Scene::Auctions:
      case Scene::Telegram:
      case Scene::Parts:{
        int x=20,y=180;
        for(size_t i=0;i<market.size();++i){
          SDL_Rect r{x,y+(int)i*110,760,100};
          if(hit(r,mx,my)){
            Item it = market[i];
            int price = (scene==Scene::Auctions)? std::max(20,(int)std::floor(it.ask*frand(0.6f,0.85f))) : it.ask;
            if(cash>=price){ cash -= price; it.cost = price; it.ask = (int)std::floor(price * frand(1.2f,1.7f)); inventory.push_back(it); market.erase(market.begin()+i); save(); }
            return;
          }
        }
      }break;
      case Scene::Map:{
        std::vector<SDL_Rect> tiles = {{20,190,380,100},{420,190,380,100},{820,190,380,100},{20,310,380,100},{420,310,380,100},{820,310,380,100}};
        std::vector<std::string> names = {"NYC • Diamond District","London • Hatton Garden","London • Mayfair","Paris • Le Marais","Flea / Antique","Online • Forums/Telegram"};
        for(size_t i=0;i<tiles.size();++i) if(hit(tiles[i],mx,my)){ location=names[i]; generateMarket(); scene=Scene::Market; save(); return; }
      }break;
      case Scene::Detail:{
        if(hit(btnBack.r,mx,my)){ scene=Scene::Market; selected=-1; return; }
        SDL_Rect hag{300,320,140,40}, buy{450,320,140,40};
        if(selected>=0 && selected < (int)market.size()){
          Item it = market[selected];
          if(hit(hag,mx,my)){
            int price = std::max(20,(int)std::floor(it.ask * frand(0.78f,0.95f)));
            if(cash>=price){ cash-=price; it.cost=price; it.ask=(int)std::floor(price*frand(1.2f,1.7f)); inventory.push_back(it); market.erase(market.begin()+selected); scene=Scene::Market; selected=-1; save(); }
            return;
          }
          if(hit(buy,mx,my)){
            if(cash>=it.ask){ cash-=it.ask; it.cost=it.ask; it.ask=(int)std::floor(it.ask*frand(1.1f,1.6f)); inventory.push_back(it); market.erase(market.begin()+selected); scene=Scene::Market; selected=-1; save(); }
            return;
          }
        }
      }break;
    }
  }

  void drawMarket(){
    int x=20,y=150;
    for(size_t i=0;i<market.size();++i){
      SDL_Rect r{x,y+(int)i*110,760,100}; int mx,my; SDL_GetMouseState(&mx,&my);
      drawCard(r.x,r.y,market[i], (mx>=r.x && mx<=r.x+r.w && my>=r.y && my<=r.y+r.h));
    }
    text(800,150,"Featured Dealers", ink);
    int dy=180; std::vector<std::string> ds={"Rafi’s Timepieces — Diamond District","Hatton Vintage — Hatton Garden","Marais Horlogerie — Paris","ForumConnect — Online"};
    for(auto&s:ds){ text(800,dy,s, SDL_Color{200,220,250,255}); dy+=24; }
  }
  void drawInventory(){
    text(20,150,"Your Inventory", ink);
    int x=20,y=180;
    for(size_t i=0;i<inventory.size();++i){
      SDL_Rect r{x,y+(int)i*110,760,100}; int mx,my; SDL_GetMouseState(&mx,&my);
      drawCard(r.x,r.y,inventory[i], (mx>=r.x && mx<=r.x+r.w && my>=r.y && my<=r.y+r.h));
    }
    drawBtn(btnPromote); drawBtn(btnQuick); drawBtn(btnBreak);
  }
  void drawFeed(const std::string& title){
    text(20,150,title, ink);
    int x=20,y=180;
    for(size_t i=0;i<market.size();++i){
      SDL_Rect r{x,y+(int)i*110,760,100}; int mx,my; SDL_GetMouseState(&mx,&my);
      drawCard(r.x,r.y,market[i], (mx>=r.x && mx<=r.x+r.w && my>=r.y && my<=r.y+r.h));
    }
  }
  void drawMap(){
    text(20,150,"World Map — tap to travel", ink);
    struct Tile{ SDL_Rect r; const char* a; const char* b; };
    std::vector<Tile> tiles = {
      {{20,190,380,100},"NYC • Diamond District","Rolex-heavy dealer networks"},
      {{420,190,380,100},"London • Hatton Garden","Sinn bias, vintage supply"},
      {{820,190,380,100},"London • Mayfair","Patek & Rolex dress"},
      {{20,310,380,100},"Paris • Le Marais","Dress & Cartier"},
      {{420,310,380,100},"Flea / Antique","Random gems & parts"},
      {{820,310,380,100},"Online • Forums/Telegram","Risky but hot deals"},
    };
    for(auto&t:tiles){
      fill(t.r, SDL_Color{18,26,44,255}); border(t.r, line);
      text(t.r.x+10, t.r.y+8, t.a, ink);
      text(t.r.x+10, t.r.y+38, t.b, mut);
    }
  }
  void drawDetail(){
    drawBtn(btnBack);
    if(selected<0 || selected >= (int)market.size()) return;
    Item it = market[selected];
    SDL_Rect area{20,140,1000,520}; fill(area, SDL_Color{16,24,41,255}); border(area, line);
    fill({40,160,240,240}, SDL_Color{11,15,25,255}); border({40,160,240,240}, SDL_Color{42,57,90,255});
    text(300,160,it.name, ink);
    text(300,196, it.brand + (it.ref.size()? " ref "+it.ref:"") + " • " + it.cond, mut);
    text(300,226, it.risk, warn);
    text(300,260, "Ask: " + money(it.ask) + "   (est. cost " + money((int)std::floor(it.ask*0.7)) + ")", SDL_Color{120,220,160,255});
    Button hag{{300,320,140,40},"Haggle"}; Button buy{{450,320,140,40},"Buy"};
    drawBtn(hag); drawBtn(buy);
  }

  void step(){
    SDL_SetRenderDrawColor(ren, 10,14,20,255); SDL_RenderClear(ren);
    header(); navBar();
    switch(scene){
      case Scene::Market:    drawMarket(); break;
      case Scene::Inventory: drawInventory(); break;
      case Scene::Auctions:  drawFeed("Auction House — click to bid"); break;
      case Scene::Telegram:  drawFeed("Telegram Drops — use escrow"); break;
      case Scene::Parts:     drawFeed("Parts Bazaar"); break;
      case Scene::Map:       drawMap(); break;
      case Scene::Detail:    drawDetail(); break;
    }
    SDL_RenderPresent(ren);
  }

  void run(){
    bool running=true;
    while(running){
      SDL_Event e;
      while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT) running=false;
        if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){ click(e.button.x, e.button.y); }
        if(e.type==SDL_KEYDOWN){
          if(e.key.keysym.sym==SDLK_r) { refreshMarket(); save(); }
          if(e.key.keysym.sym==SDLK_n) { nextDay(); save(); }
          if(e.key.keysym.sym==SDLK_s) { save(); }
          if(e.key.keysym.sym==SDLK_l) { load(); }
        }
      }
      step();
      SDL_Delay(16);
    }
    save();
  }
};

int main(int argc, char** argv){ Game g; if(!g.init()) return 1; g.run(); return 0; }

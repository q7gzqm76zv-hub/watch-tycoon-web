# WebAssembly build kit for Watch Market Tycoon
Build your C++ game into a single `index.html` that runs on iPhone/Chrome/Safari, offline, fullscreen, with save/load.

## Build
1) Install Emscripten (emsdk). See https://emscripten.org/docs/getting_started
2) In this folder:
   ```bash
   bash build.sh
   ```
3) Open `dist/index.html` to test. For offline caching, host `dist/` over HTTPS (GitHub Pages / Netlify).

## iPhone install
- Open your hosted URL in Safari → Share → **Add to Home Screen**. It saves progress in localStorage.      
   

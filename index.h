const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>BATHROOM FAN</title>

<style>
    :root {
        --bg-color: #f0f2f5;
        --panel-bg: #ffffff;
        --accent: #3498db;
        --text-main: #2c3e50;
        --text-secondary: #7f8c8d;
        --success: #27ae60;
    }

    * { box-sizing: border-box; }

    body, html {
        margin: 0; padding: 0; height: 100%%;
        font-family: 'Segoe UI', system-ui, sans-serif;
        background-color: var(--bg-color);
        color: var(--text-main);
        display: flex; flex-direction: column;
        overflow: hidden; /* Запобігаємо зайвій прокрутці */
    }

    header {
        background: var(--text-main);
        color: white; padding: 12px;
        text-align: center; font-weight: bold;
        font-size: 1rem; letter-spacing: 1px;
    }

    main {
        display: flex;
        flex-direction: row; /* Завжди в ряд */
        flex: 1;
        width: 100%%;
    }

    section {
        flex: 1;
        display: flex; flex-direction: column;
        justify-content: center; align-items: center;
        position: relative; background: var(--panel-bg);
        border: 0.5px solid #e1e4e8;
        padding: 10px;
        min-width: 0; /* Дозволяє флекс-елементам стискатися менше за вміст */
    }

    .controls-pane { background: #fafbfc; }
    .target_hum:not(.active){
        /* visibility: hidden; */
        display: none;
    }
    .fan-icon {
        position: absolute; width: 90%%; 
        max-height: 90%%; height: auto;
        color: rgb(0, 152, 255);
        pointer-events: none; 
        z-index: 0;
        opacity: 0.15;
    }
    .rings {
        fill: none;
        stroke: #dbdbdb;
    }
    .blades { 
        animation: spin 120s linear infinite; 
        transform-origin: center;
    }
    @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }

    /* Текстові блоки */
    .display { z-index: 100; text-align: center; width: 100%%; }

    .signs{font-size: 1.6rem;font-weight: 400;}
    
    .hum-value,.temp-value{font-size: 2rem;font-weight: 800;}
    .hum-before-timer{font-size: 75%%;font-weight: 500;}

    .measure-values{
        margin: 10px auto;
        border-radius: 10px;
        max-width: 300px;
    }

    .heating{
        box-shadow: inset 0px -25px 20px -15px orange;
    }

    .countdown-box {
        max-width: 300px; margin: 10px auto;
        padding: 5px 10px;
        background: #f1f3f5; border-radius: 20px;
        font-size: 1rem; font-weight: bold;
    }

    /* Кнопки */
    .btn-label { margin-bottom: 15px; font-size: 0.7rem; font-weight: bold; color: var(--text-secondary); }
    
    .btn-grid {
        display: grid;
        grid-template-columns: 1fr;
        gap: 20px; width: 90%%;
        max-width: 100px;
    }

    button {
        padding: 15px 5px; border: none; border-radius: 8px;
        background: var(--accent); color: white;
        font-weight: bold; font-size: 1rem;
        cursor: pointer; box-shadow: 0 3px 0 #2980b9;
    }
    button:active:not(:disabled) { transform: translateY(2px); box-shadow: 0 1px 0 #2980b9; }
    button:disabled { opacity: 0.5; }

    footer { background: #34495e; color: #bdc3c7; text-align: center; padding: 6px; font-size: 0.7rem; }

    /* === МОБІЛЬНА АДАПТАЦІЯ (ЗБЕРІГАЄМО 2 ПЛОЩИНИ) === */
    @media (max-width: 480px) {
        .measure-values > span {display: block;}
        .temp-value { font-size: 2rem; } /* Зменшуємо температуру */
        .hum-value { font-size: 2rem; }
        .btn-grid { grid-template-columns: 1fr; } /* Кнопки стають одна під одну у своїй колонці */
        button { padding: 10px 5px; font-size: 1rem; }
        .countdown-box { font-size: 1rem; min-width: auto; }
        header { font-size: 1rem; padding: 8px; }
    }
</style>
</head>
<body>
<header>BATHROOM FAN</header>
<main>
    <section>
        <svg class="fan-icon" viewBox="0 0 100 100">
            <g class="rings">
                <circle cx="50" cy="50" r="29"  />
                <circle cx="50" cy="50" r="40" />
            </g>

            <g id="fan-blades" class="blades" fill="currentColor">
                <circle cx="50" cy="50" r="4"/>
                <path d="M50 50 C43 40, 43 12, 50 12 C57 12, 57 40, 50 50" />
                <path d="M50 50 C43 40, 43 12, 50 12 C57 12, 57 40, 50 50" transform="rotate(90 50 50)" />
                <path d="M50 50 C43 40, 43 12, 50 12 C57 12, 57 40, 50 50" transform="rotate(180 50 50)" />
                <path d="M50 50 C43 40, 43 12, 50 12 C57 12, 57 40, 50 50" transform="rotate(270 50 50)" />
            </g>
        </svg>
        <div class="display">
            <div class="measure-values">
                <span class="hum-value"><span id="hum">%HUM%</span><span class="signs">&percnt;</span> </span>
                <span class="temp-value"><span id="temp">%TEMP%</span><span class="signs">°C</span></span>
            </div>
            <div mode="%STATE%" class="countdown-box">
                <span class="target_hum">
                  <span><span id="target_hum">%TARGET_HUM%</span>&percnt;</span><span class="hum-before-timer"> або </span>
                </span>
                <span id="timer">Вимкнутий</span>
            </div>
        </div>
    </section>

    <section class="controls-pane">
        <div class="btn-label">Керування</div>
        <div class="btn-grid">
            <button onclick="setFanTimer(this, 15)">15m</button>
            <button onclick="setFanTimer(this, 2*60)">2h</button>
            <button onclick="setFanTimer(this, 6*60)">6h</button>
            <button onclick="setFanTimer(this, 0)">OFF</button>
        </div>
    </section>
</main>

<footer><p>*при різкому зільшені вологості працює витяжка до повернення волості у початкові значення але не більше 12 годин</p></footer>

<script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    var interval;
    var timer_sec = 0;

    document.querySelectorAll('button').forEach((e)=>e.disabled=true);
    
    window.addEventListener('load', onLoad);
    
    function setFanTimer(e, min) {
        e.disabled=true;
        let sec = min*60;
        console.log(e);
        sendTimer(sec);
        doTimer(sec);
    }
    function doTimer(s) {
        if (s - timer_sec > 2 || timer_sec - s > 2){
            clearInterval(interval);
            timer_sec = s;
            const fan = document.getElementById('fan-blades');
            const timer = document.getElementById('timer');
            fan.style.animationDuration = "0.6s"; 
    
            function update() {
                let h = Math.floor(timer_sec / 3600);
                let m = Math.floor((timer_sec %% 3600) / 60);
                let s = timer_sec %% 60;
                let displayM = m < 10 ? '0' + m : m;
                let displayS = s < 10 ? '0' + s : s;
                timer.innerText = h > 0 ? `${h}:${displayM}:${displayS}` : `${displayM}:${displayS}`;
                if (timer_sec <= 0) {
                    clearInterval(interval);
                    timer.innerText = "";
                    fan.style.animationDuration = "120s";
                    timer.innerText = "Вимкнутий";    
                }
                timer_sec--;
            }
            update();
            interval = setInterval(update, 1000);
        }
    }
    
    function onLoad(event) {
        initWebSocket();
    }
    function initWebSocket() {
        websocket = new WebSocket(gateway);
        websocket.onopen    = onOpen;
        websocket.onclose   = onClose;
        websocket.onmessage = onMessage;
    }
    function onOpen(event) {
      document.querySelectorAll('button').forEach((e)=>e.disabled=false);
    }
    function onClose(event) {
      document.querySelectorAll('button').forEach((e)=>e.disabled=true);
        setTimeout(initWebSocket, 2000);
    }
    function onMessage(event) {
        let resp = JSON.parse(event.data);
        console.log(resp);
        document.getElementById('temp').innerHTML = resp.temp;
        document.getElementById('hum').innerHTML = resp.hum;
        document.getElementById('target_hum').innerHTML = parseInt(resp.target_hum);
        setMode(resp.state);
        doTimer(parseInt(resp.off_timer));
        if (resp.heating === '1') document.querySelector('.measure-values').classList.add('heating');
        else document.querySelector('.measure-values').classList.remove('heating');
        document.querySelectorAll('button:disabled').forEach((e)=>e.disabled=false);
    }
    function setMode(mode){
        let status_el = document.querySelector(".measure-values");
        let _mode = status_el.getAttribute("mode");
        if (_mode !== mode){
            status_el.setAttribute("mode", mode);
            const target_hum = document.querySelector('.target_hum');
            switch (mode) {
                case "1":
                  console.log("mode high humidity");
                  target_hum.classList.add('active');
                  break;
                case "2":
                  console.log("mode by timer");
                  break;
                case "3":
                  console.log("mode off");
                  target_hum.classList.remove('active');
                  break;
                case "0":
                  console.log("Sensor error");
            }
        }
    }
    function sendTimer(sec) {
    let data = '{\"off_timer\":\"'+sec+'\"}';
    console.log(data);
    websocket.send(data)
  }
</script>

</body>
</html>)rawliteral";

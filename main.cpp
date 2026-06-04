#include <WiFi.h>
#include <WebServer.h>

// --- Pin Assignments ---
const int PIN_DATA = 12;
const int PIN_BCK  = 14;
const int PIN_WCK  = 27;
const int PIN_ADC  = 34; // Ensure input is 0 - 3.3V max!

// --- Web Server Setup ---
WebServer server(80);

// --- The HTML/CSS/JS (Self-Contained) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PCM1702 Tester</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f4f4f9; color: #333; margin: 0; padding: 20px; }
        .container { max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
        h1 { text-align: center; color: #0056b3; }
        .btn-group { display: flex; justify-content: center; gap: 10px; margin: 20px 0; flex-wrap: wrap; }
        button { padding: 12px 24px; font-size: 16px; font-weight: bold; border: none; border-radius: 5px; cursor: pointer; color: white; transition: 0.3s; }
        #btn-test { background-color: #28a745; } #btn-test:hover { background-color: #218838; }
        #btn-test:disabled { background-color: #6c757d; cursor: not-allowed; }
        #btn-dl { background-color: #17a2b8; } #btn-dl:hover { background-color: #138496; }
        #btn-print { background-color: #6c757d; } #btn-print:hover { background-color: #5a6268; }
        .canvas-container { width: 100%; overflow-x: auto; background: #222; border-radius: 5px; margin-top: 20px;}
        canvas { display: block; width: 100%; min-width: 600px; height: 300px; }
        .status { text-align: center; font-weight: bold; margin-top: 10px; color: #d9534f; }
        
        /* Hide buttons when printing */
        @media print {
            body { background: white; }
            .container { box-shadow: none; }
            .btn-group, .status { display: none; }
            .canvas-container { background: white; border: 1px solid #ccc; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>PCM1702 Diagnostic Rig</h1>
        
        <div class="btn-group">
            <button id="btn-test" onclick="runTest()">Start Chip Test</button>
            <button id="btn-dl" onclick="downloadCSV()" disabled>Download CSV</button>
            <button id="btn-print" onclick="window.print()" disabled>Print Results</button>
        </div>
        
        <div class="status" id="status-text">Ready. Insert chip and start test.</div>

        <div class="canvas-container">
            <canvas id="graphCanvas" width="800" height="300"></canvas>
        </div>
    </div>

    <script>
        let testData = [];

        async function runTest() {
            const btn = document.getElementById('btn-test');
            const status = document.getElementById('status-text');
            const dlBtn = document.getElementById('btn-dl');
            const printBtn = document.getElementById('btn-print');
            
            btn.disabled = true;
            dlBtn.disabled = true;
            printBtn.disabled = true;
            status.style.color = '#f0ad4e';
            status.innerText = "Running test sweep... please wait.";

            try {
                // Call the ESP32 endpoint. No page refresh needed.
                const response = await fetch('/run_test');
                testData = await response.json();
                
                drawGraph(testData);
                
                status.style.color = '#28a745';
                status.innerText = "Test Complete! Graph updated.";
                dlBtn.disabled = false;
                printBtn.disabled = false;
            } catch (err) {
                status.style.color = '#d9534f';
                status.innerText = "Test failed to communicate with ESP32.";
            } finally {
                btn.disabled = false;
            }
        }

        // Custom HTML5 Canvas Plotter (No external libraries needed)
        function drawGraph(data) {
            const canvas = document.getElementById('graphCanvas');
            const ctx = canvas.getContext('2d');
            
            // Clear previous graph
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            if(data.length === 0) return;

            const w = canvas.width;
            const h = canvas.height;
            const dx = w / (data.length - 1);
            const maxAdcVal = 4095; // ESP32 12-bit ADC max

            // Draw Grid
            ctx.strokeStyle = '#444';
            ctx.lineWidth = 1;
            ctx.beginPath();
            for(let i=0; i<=4; i++) {
                let y = (h / 4) * i;
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
            }
            ctx.stroke();

            // Draw Data Line
            ctx.beginPath();
            ctx.strokeStyle = '#00ffcc';
            ctx.lineWidth = 2;
            
            for(let i = 0; i < data.length; i++) {
                const x = i * dx;
                // Invert Y so higher voltage is at the top
                const y = h - ((data[i] / maxAdcVal) * h);
                
                if(i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        function downloadCSV() {
            if(testData.length === 0) return;
            let csvContent = "data:text/csv;charset=utf-8,Step,ADC_Value\n";
            testData.forEach((val, index) => {
                csvContent += `${index},${val}\n`;
            });
            const encodedUri = encodeURI(csvContent);
            const link = document.createElement("a");
            link.setAttribute("href", encodedUri);
            link.setAttribute("download", "PCM1702_Test_Results.csv");
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }
    </script>
</body>
</html>
)rawliteral";

// --- Hardware Interfacing Functions ---

// Bit-bang 20-bit two's complement value to PCM1702
void writePCM1702(int32_t val) {
    // Bring Word Clock LOW for data loading
    digitalWrite(PIN_WCK, LOW);

    // Shift out 20 bits (MSB first)
    for(int i = 19; i >= 0; i--) {
        // Extract the i-th bit
        int bit = (val >> i) & 1;
        digitalWrite(PIN_DATA, bit);
        
        // Pulse Bit Clock
        digitalWrite(PIN_BCK, HIGH);
        delayMicroseconds(1);
        digitalWrite(PIN_BCK, LOW);
        delayMicroseconds(1);
    }

    // Latch data with Word Clock
    digitalWrite(PIN_WCK, HIGH);
    delayMicroseconds(5); 
}

// --- Web Server Endpoints ---

void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleTest() {
    // Test parameters
    const int numSteps = 250;
    String jsonOutput = "[";

    for(int i = 0; i < numSteps; i++) {
        // Map step to full 20-bit two's complement range
        // Range: -524288 to +524287
        int32_t dacValue = map(i, 0, numSteps - 1, -524288, 524287);
        
        // Write to PCM1702
        writePCM1702(dacValue);
        
        // Delay to allow your I/V Op-Amp to settle
        delay(2); 
        
        // Read the result from the ADC
        int adcValue = analogRead(PIN_ADC);
        
        // Append to JSON array string
        jsonOutput += String(adcValue);
        if (i < numSteps - 1) {
            jsonOutput += ",";
        }
    }
    
    jsonOutput += "]";
    
    // Send JSON back to the browser
    server.send(200, "application/json", jsonOutput);
}

// --- Setup and Loop ---

void setup() {
    Serial.begin(115200);

    // Configure Pins
    pinMode(PIN_DATA, OUTPUT);
    pinMode(PIN_BCK, OUTPUT);
    pinMode(PIN_WCK, OUTPUT);
    
    // Set initial pin states
    digitalWrite(PIN_DATA, LOW);
    digitalWrite(PIN_BCK, LOW);
    digitalWrite(PIN_WCK, HIGH);

    // Start Access Point
    Serial.println("Starting Access Point...");
    WiFi.softAP("PCM1702");
    
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP()); // Usually 192.168.4.1

    // Route web requests
    server.on("/", HTTP_GET, handleRoot);
    server.on("/run_test", HTTP_GET, handleTest);

    // Start server
    server.begin();
    Serial.println("Web server started.");
}

void loop() {
    // Handle web clients
    server.handleClient();
}

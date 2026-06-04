Before we dive into the code, I need to warn you about a critical hardware reality: The ESP32’s Analog-to-Digital Converter (ADC) pins have an absolute maximum input voltage of 3.3V, and they cannot tolerate negative voltages.

Because you are using a split-rail supply and an op-amp for the I/V stage, your op-amp could swing from -5V to +5V. You must ensure your I/V output is biased or level-shifted so it stays strictly within 0V to 3.3V, otherwise you will permanently destroy the ESP32!


The Architecture
The Network: The ESP32 will host its own Wi-Fi network named PCM1702.

The Web UI: Because your phone won't have internet access while connected to the ESP32, we cannot use external libraries like Chart.js or Bootstrap. Everything (HTML, CSS, JS, and an HTML5 Canvas graphing engine) is completely self-contained in the ESP32's memory.

The Signal: When you click "Start Test", the ESP32 bit-bangs 250 steps across the PCM1702's full 20-bit range (-524288 to 524287). It pauses slightly at each step, reads the analog voltage from your op-amp, and builds a JSON array.

The Result: The array is sent back to the browser via AJAX (no page refresh required), plotted on a responsive graph, and allows for CSV downloading or printing.

Pin Assignments
Connect your ESP32 to the PCM1702 logic pins and your Op-Amp output as follows:
Data (DATA): GPIO 12
Bit Clock (BCK): GPIO 14
Word Clock / Latch (WCK): GPIO 27
Op-Amp Output (ADC): GPIO 34 (Must be clamped/biased to 0-3.3V!)


The ESP32 Code (Arduino IDE)
You only need one file. Copy and paste this entirely into your Arduino IDE, select your ESP32 board, and upload.

How to Use the Rig
Flash the ESP32: Upload the code via the Arduino IDE as described above.

Connect: Open your phone or laptop's Wi-Fi settings and connect to the network named PCM1702. (It will not require a password. Ignore any "No Internet" warnings from your OS).

Open the App: Open a web browser and navigate to http://192.168.4.1.

Run the Test: Seat your PCM1702 in the socket, ensure power is applied to your split rails, and tap "Start Chip Test".

Interpret the Results: * A genuine PCM1702 will yield a smooth, diagonal line from bottom-left to top-right on the canvas (assuming your I/V stage is non-inverting).
A fake 16-bit chip will look like a chunky staircase.
A dead or incompatible delta-sigma chip will look like pure random static or a flatline.

Archive: Hit "Download CSV" to save the raw ADC data points, or "Print" to save a PDF of the chart with the buttons automatically hidden for a clean page layout.



// KTech Smart Room Dashboard

const MQTT_URL = "ws://broker.hivemq.com:8000/mqtt";
const MQTT_TOPIC = "ktech/smartroom/demo2019/sensors";
const MQTT_COMMAND_TOPIC = "ktech/smartroom/demo2019/commands";
const LIGHT_THRESHOLD = 1979;

let mqttClient = null;

// Track previous device states
let previousLed = null;
let previousBuzzer = null;

// Send commands to ESP32
function controlDevice(device, state) {
    if (!mqttClient || !mqttClient.connected) {
        alert("MQTT is not connected!");
        return;
    }

    const command = {
        device: device,
        state: state
    };

    mqttClient.publish(
        MQTT_COMMAND_TOPIC,
        JSON.stringify(command),
        function(error) {
            if (error) {
                console.error("Command publishing failed:", error);
            } else {
                console.log("Command sent:", command);
            }
        }
    );
}

// Update current date
document.getElementById("currentDate").textContent =
    new Date().toLocaleDateString("en-US", {
        weekday: "long",
        year: "numeric",
        month: "long",
        day: "numeric"
    });

// Update dashboard time
function updateTime() {
    document.getElementById("lastUpdate").textContent =
        "Last updated: " + new Date().toLocaleTimeString();
}

// Connection status
function setConnectionStatus(message, connected) {
    const status = document.getElementById("connectionStatus");

    status.innerHTML =
        `<span class="status-dot"></span>${message}`;

    status.style.color = connected ? "#4ade80" : "#f87171";
}

// Update dashboard with sensor readings
function updateDashboard(data) {

    const isDark = data.lightValue < LIGHT_THRESHOLD;
    const motion = data.motion;

    // Temperature
    document.getElementById("temperature").textContent =
        data.temperature + " °C";

    // Humidity
    document.getElementById("humidity").textContent =
        data.humidity + " %";

    // Light status
    const light = document.getElementById("lightStatus");

    light.textContent = isDark ? "Dark" : "Bright";
    light.style.color = isDark ? "#f59e0b" : "#4ade80";

    // Motion status
    const motionStatus = document.getElementById("motionStatus");

    motionStatus.textContent =
        motion ? "Detected" : "Not Detected";

    motionStatus.style.color =
        motion ? "#f87171" : "#4ade80";

    // Actual LED status from ESP32
    const led = document.getElementById("ledStatus");

    if (typeof data.led === "boolean") {
        led.textContent = data.led ? "ON" : "OFF";
        led.style.color = data.led ? "#4ade80" : "#f87171";

        if (previousLed !== null && data.led !== previousLed) {
            addActivity(data.led ? "LED turned ON" : "LED turned OFF");
        }

        previousLed = data.led;
    }

    // Actual buzzer status from ESP32
    const buzzer = document.getElementById("buzzerStatus");

    if (typeof data.buzzer === "boolean") {
        buzzer.textContent = data.buzzer ? "ON" : "OFF";
        buzzer.style.color = data.buzzer ? "#4ade80" : "#f87171";

        if (previousBuzzer !== null &&
            data.buzzer !== previousBuzzer) {
            addActivity(
                data.buzzer ? "Buzzer turned ON" : "Buzzer turned OFF"
            );
        }

        previousBuzzer = data.buzzer;
    }

    // PIR status
    const pir = document.getElementById("pirStatus");

    pir.textContent =
        motion ? "Motion Detected" : "No Motion";

    pir.style.color =
        motion ? "#f87171" : "#4ade80";

    // Update history, charts and time
    checkActivities(data);
    updateCharts(data);
    updateTime();

    console.log("Dashboard updated:", data);
}

// CHART DATA

let temperatureChart;
let humidityChart;

const chartLabels = [];
const temperatureData = [];
const humidityData = [];

function createCharts() {

    temperatureChart = new Chart(
        document.getElementById("temperatureChart"),
        {
            type: "line",
            data: {
                labels: chartLabels,
                datasets: [{
                    label: "Temperature (°C)",
                    data: temperatureData,
                    borderColor: "#f87171",
                    backgroundColor: "rgba(248,113,113,0.15)",
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                animation: false
            }
        }
    );

    humidityChart = new Chart(
        document.getElementById("humidityChart"),
        {
            type: "line",
            data: {
                labels: chartLabels,
                datasets: [{
                    label: "Humidity (%)",
                    data: humidityData,
                    borderColor: "#38bdf8",
                    backgroundColor: "rgba(56,189,248,0.15)",
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                animation: false
            }
        }
    );
}

function updateCharts(data) {

    const time = new Date().toLocaleTimeString();

    chartLabels.push(time);
    temperatureData.push(data.temperature);
    humidityData.push(data.humidity);

    // Keep latest 10 readings
    if (chartLabels.length > 10) {
        chartLabels.shift();
        temperatureData.shift();
        humidityData.shift();
    }

    temperatureChart.update();
    humidityChart.update();
}

// Initialize charts
if (typeof Chart !== "undefined") {
    createCharts();
    console.log("Charts initialized successfully!");
} else {
    console.error("Chart.js library is not loaded!");
}

// ACTIVITY HISTORY

let previousDark = null;
let previousMotion = null;
let previousTemperature = null;
let previousHumidity = null;

function addActivity(message) {

    const activityList = document.getElementById("activityList");

    const emptyMessage =
        activityList.querySelector(".activity-empty");

    if (emptyMessage) {
        emptyMessage.remove();
    }

    const item = document.createElement("li");

    const description = document.createElement("span");
    description.textContent = message;

    const time = document.createElement("span");
    time.className = "activity-time";
    time.textContent = new Date().toLocaleTimeString();

    item.appendChild(description);
    item.appendChild(time);

    activityList.prepend(item);

    // Keep latest 15 activities
    while (activityList.children.length > 15) {
        activityList.lastElementChild.remove();
    }

    console.log("Activity:", message);
}

function checkActivities(data) {

    const isDark = data.lightValue < LIGHT_THRESHOLD;
    const motion = data.motion;

    if (previousDark === null) {
        addActivity("Room monitoring started");
    } else if (isDark !== previousDark) {
        addActivity(
            isDark ? "Room became dark" : "Room became bright"
        );
    }

    if (previousMotion !== null && motion !== previousMotion) {
        addActivity(
            motion ? "Motion detected!" : "Motion stopped"
        );
    }

    if (previousTemperature !== null &&
        Math.abs(data.temperature - previousTemperature) >= 1) {

        addActivity(
            "Temperature changed to " + data.temperature + " °C"
        );
    }

    if (previousHumidity !== null &&
        Math.abs(data.humidity - previousHumidity) >= 5) {

        addActivity(
            "Humidity changed to " + data.humidity + "%"
        );
    }

    previousDark = isDark;
    previousMotion = motion;
    previousTemperature = data.temperature;
    previousHumidity = data.humidity;
}

// CONNECT TO MQTT

function connectMQTT() {

    if (typeof mqtt === "undefined") {
        setConnectionStatus("MQTT Library Missing", false);
        console.error("MQTT library not loaded");
        return;
    }

    setConnectionStatus("Connecting...", false);

    mqttClient = mqtt.connect(MQTT_URL, {
        clientId: "KTechDashboard_" +
            Math.random().toString(16).substring(2, 10),
        reconnectPeriod: 3000,
        connectTimeout: 10000
    });

    mqttClient.on("connect", function () {

        console.log("MQTT Connected!");

        setConnectionStatus("MQTT Connected", true);

        mqttClient.subscribe(MQTT_TOPIC, function(error) {

            if (error) {
                console.error("Subscription error:", error);
            } else {
                console.log("Subscribed to:", MQTT_TOPIC);
            }
        });
    });

    mqttClient.on("message", function(topic, message) {

        try {
            const data = JSON.parse(message.toString());

            console.log("Received sensor data:", data);

            updateDashboard(data);

        } catch (error) {
            console.error("Invalid sensor data:", error);
        }
    });

    mqttClient.on("error", function(error) {

        console.error("MQTT Error:", error);
        setConnectionStatus("Connection Error", false);
    });

    mqttClient.on("close", function() {
        setConnectionStatus("Disconnected", false);
    });
}

// START DASHBOARD

updateTime();
setInterval(updateTime, 1000);

connectMQTT();

console.log("KTech Smart Room Dashboard Started!");

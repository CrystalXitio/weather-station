const SHEET_CSV_URL = "https://docs.google.com/spreadsheets/d/e/2PACX-1vSPXP92EqtheXrh3Z20EyP8SicTPMdIuM1Lcn12xnfhTKZhzXnv8Mrp4AXB5D8zp0VUPVcZPmaeSnzK/pub?output=csv";

let globalRawData = [];
const chartInstances = {};

const sensorConfig = {
    temp: { colIdx: 2, color: '#ff3e3e', unit: '°C' },
    hum: { colIdx: 3, color: '#00d2ff', unit: '%' },
    co2: { colIdx: 5, color: '#32d74b', unit: 'PPM' },
    lux: { colIdx: 4, color: '#ffd60a', unit: 'LUX' },
    press: { colIdx: 7, color: '#94a3b8', unit: 'hPa' }
};

/**
 * Initializes DOM elements with a loading state prior to the initial payload fetch.
 */
function setLoadingState() {
    Object.keys(sensorConfig).forEach(id => {
        const el = document.getElementById('val-' + id);
        if (el && el.innerText === '--') {
            el.classList.add('loading');
            el.innerText = '00.0';
        }
    });
}

/**
 * Removes the loading state CSS class from all active sensor elements.
 */
function removeLoadingState() {
    Object.keys(sensorConfig).forEach(id => {
        const el = document.getElementById('val-' + id);
        if (el) {
            el.classList.remove('loading');
        }
    });
}

/**
 * Fetches and parses the CSV payload from the published Google Sheet endpoint.
 */
function fetchData() {
    Papa.parse(SHEET_CSV_URL, {
        download: true,
        header: false,
        complete: function(results) {
            const data = results.data;
            if(data.length < 2) return; 

            globalRawData = data.filter(row => row.length >= 8 && row[0] !== "");
            
            removeLoadingState();
            
            const latest = globalRawData[globalRawData.length - 1];
            document.getElementById('val-temp').innerText = latest[2] || '--';
            document.getElementById('val-hum').innerText = latest[3] || '--';
            document.getElementById('val-lux').innerText = latest[4] || '--';
            document.getElementById('val-co2').innerText = latest[5] || '--';
            document.getElementById('val-press').innerText = latest[7] || '--';

            Object.keys(sensorConfig).forEach(id => renderSpecificChart(id));
        },
        error: function(err) {
            console.error("Error fetching data:", err);
        }
    });
}

/**
 * Renders or updates a specific sensor's chart based on its selected timeframe.
 * @param {string} id - The sensor identifier mapped in sensorConfig.
 */
function renderSpecificChart(id) {
    if (globalRawData.length === 0) return;

    const rangeVal = document.getElementById('range-' + id).value;
    const latestTime = new Date(globalRawData[globalRawData.length - 1][0]).getTime();
    
    let filteredData = globalRawData;

    if (rangeVal !== 'max') {
        const hoursMs = parseInt(rangeVal) * 60 * 60 * 1000;
        filteredData = globalRawData.filter(row => {
            const rowTime = new Date(row[0]).getTime();
            return (latestTime - rowTime) <= hoursMs;
        });
    }

    if(filteredData.length === 0) {
        filteredData = [globalRawData[globalRawData.length - 1]];
    }

    const labels = filteredData.map(row => {
        let d = new Date(row[0]);
        let hhmm = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0');
        if (rangeVal === '1') {
            return hhmm + ':' + String(d.getSeconds()).padStart(2, '0');
        } else if (rangeVal === '168' || rangeVal === 'max') {
            return d.getDate() + '/' + (d.getMonth()+1) + ' ' + hhmm;
        }
        return hhmm; 
    });

    const plotData = filteredData.map(row => parseFloat(row[sensorConfig[id].colIdx]));

    updateChartUI(id, labels, plotData);
}

/**
 * Handles the Chart.js instance creation or mutation based on incoming payload.
 */
function updateChartUI(id, labels, data) {
    const ctx = document.getElementById('chart-' + id).getContext('2d');
    
    if (chartInstances[id]) {
        chartInstances[id].data.labels = labels;
        chartInstances[id].data.datasets[0].data = data;
        chartInstances[id].update();
        return;
    }

    chartInstances[id] = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [{
                data: data,
                borderColor: sensorConfig[id].color,
                borderWidth: 3,
                pointRadius: 0,
                pointHoverRadius: 6, 
                pointHoverBackgroundColor: '#ffffff',
                pointHoverBorderColor: sensorConfig[id].color,
                pointHoverBorderWidth: 2,
                fill: true,
                backgroundColor: sensorConfig[id].color + '15',
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            plugins: { 
                legend: { display: false },
                tooltip: {
                    backgroundColor: 'rgba(10, 15, 26, 0.95)', 
                    titleColor: '#94a3b8', 
                    titleFont: { family: "'Plus Jakarta Sans', sans-serif", size: 12 },
                    bodyColor: '#ffffff',
                    bodyFont: { family: "'JetBrains Mono', monospace", size: 15, weight: 'bold' },
                    borderColor: sensorConfig[id].color + '80', 
                    borderWidth: 1,
                    padding: 12,
                    displayColors: false, 
                    callbacks: {
                        title: function(context) { return 'Time: ' + context[0].label; },
                        label: function(context) { return 'Reading: ' + context.parsed.y + ' ' + sensorConfig[id].unit; }
                    }
                }
            },
            scales: {
                x: { 
                    display: true, 
                    grid: { display: false },
                    ticks: { 
                        color: '#64748b',
                        maxTicksLimit: 6,
                        maxRotation: 0
                    } 
                },
                y: { 
                    grid: { color: 'rgba(255,255,255,0.03)' }, 
                    ticks: { color: '#64748b' } 
                }
            }
        }
    });
}

document.addEventListener('DOMContentLoaded', () => {
    setLoadingState();
    fetchData();
    setInterval(fetchData, 10000);
});

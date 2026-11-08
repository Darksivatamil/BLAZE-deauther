/* BLAZE // TEAM DARK - detector panel */
var detRunning = false;
var detTimer = null;
var detPhases = ["IDLE", "SCANNING", "SNIFFING", "ANALYZING", "PROVING"];

function detBadge(level) {
	if (level === 2) return '<span class="red bold">[CONFIRMED]</span>';
	if (level === 1) return '<span class=meter_orange>[LIKELY]</span>';
	return '<span class=cyan>[INFO]</span>';
}

function detRender(j) {
	var list = j.findings || [];
	var html = "";
	for (var i = 0; i < list.length; i++) {
		var f = list[i];
		html += "<p>" + detBadge(f.level) + ' <span class=bold>' + esc(f.type) + "</span>";
		if (f.ssid) html += " - " + esc(f.ssid);
		html += "</p>";
		if (f.detail) html += '<p class=dim>' + esc(f.detail) + "</p>";
		if (f.confirm === true) html += '<p><button onclick="detProve(' + i + ')">PROVE</button></p>';
	}
	if (!list.length) html = '<p class=dim>no findings. air is clean.</p>';
	getE("detFindings").innerHTML = html;
}

function detApply(j) {
	detRunning = !!j.running;
	var phase = (j.phase >= 0 && j.phase <= 4) ? j.phase : 0;
	getE("detStatus").textContent = detRunning ? detPhases[phase] : "IDLE";
	getE("detRun").textContent = detRunning ? "RUNNING" : "STOPPED";
	getE("detRun").className = detRunning ? "green" : "dim";
	getE("detCount").textContent = j.count || 0;
	getE("detStart").textContent = detRunning ? "STOP DETECT" : "START SCAN";
	detRender(j);
}

function detFetch() {
	getFile("detect.json", function (res) {
		detApply(JSON.parse(res));
	}, 5000, "GET", function () { }, function () { });
}

function detToggle() {
	if (detRunning) {
		detStop();
	} else {
		getFile("detect?cmd=start", function () {
			detRunning = true;
			getE("detStart").textContent = "STOP DETECT";
			getE("detStatus").textContent = "SCANNING";
			pollDet();
		});
	}
}

function detStop() {
	getFile("detect?cmd=stop", function () {
		detRunning = false;
		getE("detStart").textContent = "START SCAN";
		getE("detStatus").textContent = "IDLE";
		getE("detRun").textContent = "STOPPED";
		getE("detRun").className = "dim";
		stopDetPoll();
	});
}

function detClear() {
	getFile("detect?cmd=clear", function () {
		detFetch();
	});
}

function detProve(i) {
	getFile("detect?cmd=confirm&i=" + i, function () {
		detFetch();
	});
}

function pollDet() {
	stopDetPoll();
	detTimer = setInterval(function () {
		getFile("detect.json", function (res) {
			detApply(JSON.parse(res));
		}, 5000, "GET", function () { }, function () { });
	}, 1000);
}

function stopDetPoll() {
	if (detTimer) { clearInterval(detTimer); detTimer = null; }
}

function load() {
	detFetch();
	pollDet();
}

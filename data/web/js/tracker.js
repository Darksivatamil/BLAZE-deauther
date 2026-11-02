/* BLAZE // TEAM DARK - tracker page logic
   GET  /blaze/tracker -> live tracker state (incl. picking + aps list)
   POST /blaze/track   -> start/stop/target/env/txp/calibrate
   Flow: reload APs -> pick one -> START TRACKING -> live view */

(function () {
	var pickListEl, pickCountEl, msgEl, msg2El, trkGoBtn;
	var histCv, histCtx;

	var sel = -1;          // highlighted AP index (client side)
	var state = null;      // last tracker state
	var poll = null;
	var tracking = false;

	function $(id) { return document.getElementById(id); }

	function msg(t) { if (msgEl) msgEl.textContent = t; }
	function msg2(t) { if (msg2El) msg2El.textContent = t; }

	function xhr(method, url, body, cb) {
		var r = new XMLHttpRequest();
		r.open(method, url, true);
		r.timeout = 6000;
		if (body) r.setRequestHeader("Content-Type", "application/json");
		r.onreadystatechange = function () {
			if (this.readyState === 4) {
				if (this.status === 200) {
					try { cb(JSON.parse(this.responseText)); }
					catch (e) { msg("bad response"); }
				} else {
					msg("device unreachable");
				}
			}
		};
		r.ontimeout = function () { msg("timeout"); };
		r.send(body ? JSON.stringify(body) : null);
	}

	function post(body) {
		msg("sending...");
		xhr("POST", "/blaze/track", body, function (j) {
			msg("");
			applyState(j);
		});
	}

	/* ===== pick list ===== */
	function renderPick() {
		var aps = (state && state.aps) ? state.aps : [];
		pickCountEl.textContent = aps.length
			? aps.length + " access point(s) found. Select one to track."
			: "No APs yet. Run a scan first.";

		if (!aps.length) {
			pickListEl.innerHTML = "<div class=pickRow><span class=dim>No targets</span></div>";
			trkGoBtn.disabled = true;
			return;
		}

		var html = "";
		for (var i = 0; i < aps.length; i++) {
			var a = aps[i];
			var on = (a.i === sel) ? " on" : "";
			html += "<div class='pickRow" + on + "' onclick='trkSelect(" + a.i + ")'>"
				+ "<span>" + esc(a.ssid) + (a.sel ? " *" : "") + "</span>"
				+ "<span class=dim>ch" + a.ch + " // " + a.rssi + "dBm</span>"
				+ "</div>";
		}
		pickListEl.innerHTML = html;
		trkGoBtn.disabled = (sel < 0);
	}

	function applyState(j) {
		state = j;

		if (j.picking && !j.active) {
			// still in pick mode
			tracking = false;
			$("pickWrap").style.display = "";
			$("liveWrap").style.display = "none";
			$("liveWrap2").style.display = "none";
			if (sel < 0 && j.aps && j.aps.length) sel = j.pickIdx;
			renderPick();
			return;
		}

		if (j.active) {
			tracking = true;
			$("pickWrap").style.display = "none";
			$("liveWrap").style.display = "";
			$("liveWrap2").style.display = "";
			$("tSsid").textContent = j.ssid || "--";
			$("tRssi").textContent = j.hasSignal ? j.rssi + "dBm" : "--";

			var d = j.dist;
			$("tDist").textContent = (d !== null && d !== undefined) ? d.toFixed(1) + "m" : "--";
			$("tMin").textContent = (j.min !== null && j.min !== undefined) ? j.min.toFixed(1) + "m" : "--";

			var dir = j.dir > 0 ? "CLOSER" : (j.dir < 0 ? "FARTHER" : "STEADY");
			$("tDir").textContent = dir;

			// gauge 0..100 from rssi -90..-30
			var w = 0;
			if (j.hasSignal) {
				w = Math.max(0, Math.min(100, (j.rssi + 90) * 100 / 60));
			}
			$("tBar").style.width = w + "%";

			$("tStatus").textContent = j.lost ? "SIGNAL LOST" :
				(j.scanning ? "re-scanning..." : "TRACKING");
			$("tEnv").textContent = j.envName + " (" + j.env + ")";
			$("tTxp").textContent = j.txp;
			drawHist(j.hist || []);
			showMessage("connected");
			return;
		}

		// idle: show pick view
		tracking = false;
		$("pickWrap").style.display = "";
		$("liveWrap").style.display = "none";
		$("liveWrap2").style.display = "none";
		renderPick();
	}

	function drawHist(hist) {
		if (!histCv) return;
		if (!histCtx) histCtx = histCv.getContext("2d");
		var w = histCv.width, h = histCv.height;
		histCtx.fillStyle = "#0a120a";
		histCtx.fillRect(0, 0, w, h);
		if (hist.length < 2) return;

		histCtx.strokeStyle = "#39ff14";
		histCtx.lineWidth = 1.5;
		histCtx.beginPath();
		for (var i = 0; i < hist.length; i++) {
			var x = i * (w - 2) / (hist.length - 1) + 1;
			var v = Math.max(-95, Math.min(-30, hist[i]));
			var y = h - 2 - ((v + 95) / 65) * (h - 4);
			if (i === 0) histCtx.moveTo(x, y); else histCtx.lineTo(x, y);
		}
		histCtx.stroke();
	}

	function load() {
		xhr("GET", "/blaze/tracker", null, applyState);
	}

	function startPoll() {
		if (poll) clearInterval(poll);
		poll = setInterval(function () {
			if (tracking) xhr("GET", "/blaze/tracker", null, applyState);
		}, 1000);
	}

	/* ===== public hooks (onclick) ===== */
	window.trkSelect = function (i) {
		sel = i;
		renderPick();
	};

	window.trkReload = function () {
		sel = -1;
		msg("reloading...");
		load();
	};

	window.trkStartScan = function () {
		msg("scanning...");
		getFile("run?cmd=scan -a", function () {
			setTimeout(function () { sel = -1; load(); }, 1800);
		});
	};

	window.trkStart = function () {
		if (sel < 0) { msg("select a target first"); return; }
		post({ on: true, target: sel });
	};

	window.trkStop = function () {
		post({ on: false });
		tracking = false;
		$("pickWrap").style.display = "";
		$("liveWrap").style.display = "none";
		$("liveWrap2").style.display = "none";
		sel = -1;
		load();
	};

	window.trkCalibrate = function () {
		post({ calibrate: true });
		msg2("calibrated @ 1m");
	};

	function boot() {
		pickListEl = $("pickList");
		pickCountEl = $("pickCount");
		msgEl = $("trkMsg");
		msg2El = $("trkMsg2");
		trkGoBtn = $("trkGo");
		histCv = $("tHist");

		load();
		startPoll();
	}

	if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", boot);
	else boot();
})();

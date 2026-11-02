/* BLAZE // TEAM DARK - shared helpers + auth + nav + matrix rain */

var BLAZE_USER = "BLAZE";
var BLAZE_PASS = "&bl@ze1234s678";

function blazePage() {
	var p = location.pathname.split("/").pop();
	return p === "" ? "index.html" : p;
}

function blazePost(url, body, cb) {
	try {
		var r = new XMLHttpRequest();
		r.open("POST", url, true);
		r.setRequestHeader("Content-Type", "application/json");
		r.timeout = 25000;
		r.onreadystatechange = function () {
			if (this.readyState === 4 && this.status === 200 && cb) cb(this.responseText);
		};
		r.send(body);
	} catch (e) { }
}

/* ===== nav =====
   Central builder injected into <div id=nav> on every page.
   Keeps KILL / CHAT removed. Marks the active page. */
var BLAZE_NAV = [
	["scan.html", "SCAN"],
	["ssids.html", "SSIDs"],
	["attack.html", "ATTACK"],
	["tracker.html", "TRACK"],
	["twin.html", "TWIN"],
	["settings.html", "SETTINGS"],
	["info.html", "INFO"]
];

function blazeNav() {
	var host = document.getElementById("nav");
	if (!host) return;
	var cur = blazePage();
	var html = '<nav><span class="brand">BLAZE</span><ul class="menu">';
	for (var i = 0; i < BLAZE_NAV.length; i++) {
		var href = BLAZE_NAV[i][0];
		var label = BLAZE_NAV[i][1];
		var active = (cur === href) ? " class=active" : "";
		html += "<li><a href=" + href + active + ">" + label + "</a></li>";
	}
	html += "</ul></nav>";
	host.innerHTML = html;
}

/* ===== auth gate =====
   Wraps every page except login.html. On failure sends the user to login. */
function blazeAuth() {
	var cur = blazePage();
	if (cur === "login.html") return true;

	var ok = null;
	try { ok = window.localStorage.getItem("blaze_auth"); } catch (e) { }
	if (ok === "1") return true;

	location.href = "login.html";
	return false;
}

/* called by the login form */
function blazeLogin(user, pass) {
	if (user === BLAZE_USER && pass === BLAZE_PASS) {
		try { window.localStorage.setItem("blaze_auth", "1"); } catch (e) { }
		location.href = "scan.html";
		return true;
	}
	return false;
}

function blazeLogout() {
	try { window.localStorage.removeItem("blaze_auth"); } catch (e) { }
	location.href = "login.html";
}

/* ===== matrix rain background (optional, enabled per page) ===== */
function blazeRain() {
	var cv = document.getElementById("matrix");
	if (!cv) return;
	var ctx = cv.getContext("2d");
	var cols, drops;

	function resize() {
		cv.width = window.innerWidth;
		cv.height = window.innerHeight;
		cols = Math.floor(cv.width / 14);
		drops = [];
		for (var i = 0; i < cols; i++) drops[i] = 1;
	}
	resize();
	window.addEventListener("resize", resize);

	var chars = "01BLAZE<>/*+-=#$%";
	function draw() {
		ctx.fillStyle = "rgba(5,7,10,0.08)";
		ctx.fillRect(0, 0, cv.width, cv.height);
		ctx.fillStyle = "#39ff14";
		ctx.font = "12px monospace";
		for (var i = 0; i < drops.length; i++) {
			var t = chars[Math.floor(Math.random() * chars.length)];
			ctx.fillText(t, i * 14, drops[i] * 14);
			if (drops[i] * 14 > cv.height && Math.random() > 0.975) drops[i] = 0;
			drops[i]++;
		}
		setTimeout(draw, 55);
	}
	draw();
}

/* boot: inject nav, gate auth, start rain on dashboard */
(function () {
	if (document.readyState === "loading") {
		document.addEventListener("DOMContentLoaded", blazeBoot);
	} else {
		blazeBoot();
	}
})();

function blazeBoot() {
	if (!blazeAuth()) return;
	blazeNav();
	if (document.getElementById("matrix")) blazeRain();
	// pages with a global load() (scan/attack/ssids/settings/twin) fetch on boot
	if (typeof load === "function") load();
}

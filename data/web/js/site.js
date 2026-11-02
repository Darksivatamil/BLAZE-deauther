/* BLAZE // TEAM DARK - minimal shared helpers (no i18n, English hardcoded) */

function getE(name) {
	return document.getElementById(name);
}

function esc(str) {
	if (str) {
		return str.toString()
			.replace(/&/g, "&amp;")
			.replace(/</g, "&lt;")
			.replace(/>/g, "&gt;")
			.replace(/"/g, "&quot;")
			.replace(/'/g, "&#39;");
	}
	return "";
}

function showMessage(msg, closeAfter) {
	if (String(msg).indexOf("ERROR") === 0) {
		getE("status").style.backgroundColor = "#d33";
		getE("status").innerHTML = "disconnected";
	} else {
		getE("status").style.backgroundColor = "#3c5";
		getE("status").innerHTML = "connected";
	}
}

function getFile(adr, callback, timeout, method, onTimeout, onError) {
	if (adr === undefined) return;
	if (callback === undefined) callback = function () { };
	if (timeout === undefined) timeout = 8000;
	if (method === undefined) method = "GET";
	if (onTimeout === undefined) {
		onTimeout = function () { showMessage("ERROR: timeout " + adr); };
	}
	if (onError === undefined) {
		onError = function () { showMessage("ERROR: " + adr); };
	}

	var request = new XMLHttpRequest();
	request.open(method, encodeURI(adr), true);
	request.timeout = timeout;
	request.ontimeout = onTimeout;
	request.onerror = onError;
	request.overrideMimeType("application/json");
	request.onreadystatechange = function () {
		if (this.readyState == 4 && this.status == 200) callback(this.responseText);
	};
	request.send();
}

/* legacy lang() - tiny embedded English map (no i18n files) */
var LANG_MAP = {
	"start": "START", "stop": "STOP", "save": "Save", "add": "Add",
	"reload": "Reload", "scan": "Scan", "ssids": "SSIDs", "attacks": "Attacks",
	"settings": "Settings", "all": "All", "channel": "Channel",
	"select_all": "Select All", "deselect_all": "Deselect All", "remove_all": "Remove All",
	"station_scan_time": "Station Scan Time", "number": "Number", "overwrite": "Overwrite",
	"add_selected": "Clone selected APs", "enable_random": "Enable Random Mode",
	"disable_random": "Disable Random Mode", "start_stop": "START / STOP",
	"wifi_off": "WiFi Off", "reboot": "Reboot", "reset": "Reset",
	"targets": "Targets", "info_span": "INFO: ",
	"setting_version": "Firmware version.",
	"setting_ssid": "AP SSID for the web interface.",
	"setting_password": "AP password (8+ chars).",
	"setting_channel": "Default WiFi channel.",
	"setting_hidden": "Hide the management AP.",
	"setting_captivePortal": "Captive portal redirect.",
	"setting_autosave": "Auto-save SSIDs, names and settings.",
	"setting_autosavetime": "Auto-save interval (ms).",
	"setting_display": "Enable the OLED display.",
	"setting_displayTimeout": "Display timeout (s, 0 = never).",
	"setting_serial": "Enable serial console.",
	"setting_serialEcho": "Echo serial input.",
	"setting_web": "Enable web interface.",
	"setting_webSpiffs": "Serve web files from LittleFS.",
	"setting_led": "Enable RGB LED.",
	"setting_maxch": "Max scan channel (US=11, EU=13, JP=14).",
	"setting_macAP": "AP MAC address override.",
	"setting_macSt": "Station MAC address override.",
	"setting_chtime": "Channel hop dwell time (ms).",
	"setting_minDeauths": "Min deauth frames for deauth LED mode.",
	"setting_attacktimeout": "Attack auto-stop timeout (s, 0 = never).",
	"setting_forcepackets": "Packet send attempts (max 255).",
	"setting_deauthspertarget": "Deauth/disassoc frames per target.",
	"setting_deauthReason": "Deauth reason code.",
	"setting_beaconchannel": "Hop channels during beacon attack.",
	"setting_beaconInterval": "Beacon interval: true = 1s, false = 100ms.",
	"setting_randomTX": "Randomize TX power for beacon/probe.",
	"setting_probesPerSSID": "Probe frames per SSID.",
	"setting_lang": "Web UI language."
};
function lang(key) { return LANG_MAP[key] !== undefined ? LANG_MAP[key] : key; }

function loadLang() {
	if (typeof load !== "undefined") load();
}

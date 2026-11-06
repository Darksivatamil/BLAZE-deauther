/* BLAZE // TEAM DARK - chat page (offline brain via POST /blaze/chat) */

function chatRender(list) {
	var el = getE("chatLog");
	if (!el) return;
	var html = "";
	for (var i = 0; i < list.length; i++) {
		var t = esc(list[i]);
		var cls = (list[i].indexOf("you>") === 0) ? "chatYou" : "chatBlaze";
		html += "<p class=" + cls + ">" + t + "</p>";
	}
	el.innerHTML = html;
	el.scrollTop = el.scrollHeight;
}

function chatLoad() {
	getFile("/blaze/chat", function (txt) {
		try { chatRender(JSON.parse(txt)); } catch (e) { }
	}, 5000);
}

function chatStance() {
	getFile("/blaze/status", function (txt) {
		try {
			var j = JSON.parse(txt);
			var s = getE("cStance"), l = getE("cLine");
			if (s) s.textContent = j.stance || "--";
			if (l) l.textContent = j.line || "--";
		} catch (e) { }
	}, 5000);
}

function chatSend() {
	var inp = getE("chatIn");
	if (!inp) return;
	var m = inp.value.replace(/^\s+|\s+$/g, "");
	if (!m) return;
	inp.value = "";
	blazePost("/blaze/chat", JSON.stringify({ msg: m }), function (txt) {
		var msgEl = getE("chatMsg");
		try {
			var j = JSON.parse(txt);
			if (msgEl) msgEl.textContent = j.ok ? "blaze replied." : "say more.";
		} catch (e) {
			if (msgEl) msgEl.textContent = "no reply.";
		}
		chatLoad();
	});
}

/* boot + poll (blaze.js calls load() when it exists) */
function load() {
	chatLoad();
	chatStance();
}
setInterval(function () {
	chatLoad();
	chatStance();
}, 4000);

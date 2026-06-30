#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>4L Telemetrie</title><style>
:root{--bg:#15140f;--card:#211e17;--ink:#ece3cf;--mut:#9a8f78;--g:#4caf6a;--o:#e3922f;--r:#e23b22}
*{box-sizing:border-box;font-family:system-ui,sans-serif}
body{margin:0;background:var(--bg);color:var(--ink)}
header{padding:14px 16px;font-weight:700;letter-spacing:.04em;border-bottom:1px solid #2c2820;display:flex;justify-content:space-between;align-items:center}
#stat{font-size:12px;color:var(--mut);font-weight:500}
main{padding:16px;display:grid;gap:14px;max-width:520px;margin:0 auto}
.alarm{background:var(--r);color:#fff;padding:12px;border-radius:10px;text-align:center;font-weight:700;display:none}
.alarm.on{display:block;animation:bl 1s steps(1) infinite}@keyframes bl{50%{opacity:.35}}
.card{background:var(--card);border-radius:12px;padding:16px}
.lbl{font-size:12px;color:var(--mut);text-transform:uppercase;letter-spacing:.08em}
.val{font-size:46px;font-weight:800;line-height:1.1;margin-top:4px}
.u{font-size:18px;color:var(--mut);font-weight:600}
.row{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.mm{font-size:12px;color:var(--mut);margin-top:6px}
.g{color:var(--g)}.o{color:var(--o)}.r{color:var(--r)}
.cfg .frow{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-top:10px;font-size:13px}
.cfg label{color:var(--mut)}
.cfg input[type=number]{width:88px;background:#15140f;border:1px solid #3a352b;color:var(--ink);border-radius:6px;padding:6px;font-size:14px}
.cfg input[type=range]{flex:1}
.btns{display:flex;gap:10px;margin-top:14px}
button{flex:1;background:var(--g);color:#0b0b08;border:0;border-radius:8px;padding:10px;font-weight:700;font-size:14px}
button.sec{background:#3a352b;color:var(--ink)}
.wake{width:100%;background:#3a352b;color:var(--ink);border:0;border-radius:10px;padding:11px;font-weight:700;font-size:14px}
.wake.on{background:var(--g);color:#0b0b08}
.topbtns{display:flex;gap:10px}.topbtns .wake{width:auto}
body.fs{overflow:hidden}
body.fs header{display:none}
body.fs main{max-width:none;min-height:100vh;padding:10px;align-content:center}
body.fs main>.card{display:none}
body.fs #wake{display:none}
body.fs .topbtns{justify-content:center}
body.fs #fs{flex:0 0 auto;width:auto;opacity:.85}
body.fs #tach{max-width:min(72vw,54vh)}
.msg{margin-top:10px;font-size:13px;color:var(--g);min-height:16px}
.log{margin:8px 0 0;max-height:220px;overflow:auto;background:#0e0d0a;border:1px solid #2c2820;border-radius:8px;padding:10px;font:12px/1.5 ui-monospace,monospace;color:var(--mut);white-space:pre-wrap;word-break:break-word}
.gauge{display:flex;justify-content:center;padding:6px 6px 2px}
#tach{width:100%;max-width:300px;height:auto;display:block}
.bezel{fill:#0e0d0a;stroke:#2c2820;stroke-width:2}
.zone{fill:none;stroke-width:9}
.zg{stroke:var(--g)}.zo{stroke:var(--o)}.zr{stroke:var(--r)}
.tick{stroke:var(--mut);stroke-width:2}.tickM{stroke:var(--ink);stroke-width:3}
.tlbl{fill:var(--ink);font:600 14px system-ui,sans-serif;text-anchor:middle;dominant-baseline:middle}
#needle{fill:#f2ead6;transform-box:view-box;transform-origin:120px 120px;transition:transform .2s linear}
.hub{fill:#f2ead6;stroke:#2c2820;stroke-width:1.5}
.rval{fill:var(--ink);font:800 34px system-ui,sans-serif;text-anchor:middle;dominant-baseline:middle}
.runit{fill:var(--mut);font:600 12px system-ui,sans-serif;text-anchor:middle;letter-spacing:.1em}
@media (orientation:landscape){
 main{max-width:920px}
 .dash{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:stretch}
 .dash .gauge{margin:0;align-items:center}
 .dash .row{grid-template-columns:1fr;align-content:center}
 #tach{max-width:340px}
}
</style></head><body>
<header><span>RENAULT 4L &middot; TELEMETRIE</span><span id="stat">...</span></header>
<main>
<div class="topbtns"><button id="wake" class="wake">Garder l'ecran allume</button><button id="fs" class="wake">Plein ecran</button></div>
<div class="alarm" id="al">&#9888; ALARME</div>
<div class="dash">
<div class="card gauge">
<svg id="tach" viewBox="0 0 240 240" aria-label="compte-tours">
<circle class="bezel" cx="120" cy="120" r="118"/>
<g id="zones"></g><g id="ticks"></g>
<polygon id="needle" points="120,40 117,120 120,133 123,120"/>
<circle class="hub" cx="120" cy="120" r="7"/>
<text id="rv" class="rval" x="120" y="156">&mdash;</text>
<text class="runit" x="120" y="178">tr/min</text>
</svg></div>
<div class="row">
<div class="card"><div class="lbl">Temp. eau</div><div class="val" id="temp">&mdash;<span class="u"> &deg;C</span></div><div class="mm" id="tmax">max &mdash;</div></div>
<div class="card"><div class="lbl">Batterie</div><div class="val" id="volt">&mdash;<span class="u"> V</span></div><div class="mm" id="vmm">&mdash; / &mdash;</div></div>
</div>
</div>
<div class="card cfg">
<div class="lbl">Reglages</div>
<div class="frow"><label>Mot de passe config</label><input id="tok" type="password"></div>
<div class="frow"><label>Temp. orange &deg;C</label><input id="cTempOk" type="number" step="1"></div>
<div class="frow"><label>Temp. rouge &deg;C</label><input id="cTempWn" type="number" step="1"></div>
<div class="frow"><label>Tension rouge bas V</label><input id="cVLo" type="number" step="0.1"></div>
<div class="frow"><label>Tension rouge haut V</label><input id="cVHi" type="number" step="0.1"></div>
<div class="frow"><label>Calibration tension</label><input id="cVCal" type="number" step="0.001"></div>
<div class="frow"><label>Luminosite <span id="bv">100</span>%</label><input id="cBright" type="range" min="5" max="100" step="5"></div>
<div class="btns"><button id="save">Enregistrer</button><button id="rst" class="sec">Reset min/max</button></div>
<div id="msg" class="msg"></div>
</div>
<div class="card">
<div class="lbl">Mise a jour firmware (OTA)</div>
<div class="frow"><label>Fichier .bin</label><input id="fw" type="file" accept=".bin"></div>
<div class="btns"><button id="fwbtn">Flasher</button><button id="scrbtn" class="sec">Reinit ecran</button><button id="rebootbtn" class="sec">Redemarrer</button></div>
<div id="fwmsg" class="msg"></div>
<div class="mm">Mot de passe = champ « config » ci-dessus. Le cadran redemarre apres le flash.</div>
</div>
<div class="card">
<div class="lbl">Console serie</div>
<pre id="log" class="log">...</pre>
</div>
</main>
<script>
function $(i){return document.getElementById(i)}
function col(e,c){e.classList.remove('g','o','r');if(c)e.classList.add(c)}
var TACH=(function(){
 var NS=$('tach').namespaceURI,CX=120,CY=120,RMAX=7000,A0=-135,A1=135;
 function pol(r,a){var d=a*Math.PI/180;return[CX+r*Math.sin(d),CY-r*Math.cos(d)]}
 function ang(v){v=v<0?0:(v>RMAX?RMAX:v);return A0+(v/RMAX)*(A1-A0)}
 function arc(r,a,b,cls){var p=pol(r,a),q=pol(r,b),la=(b-a)>180?1:0,e=document.createElementNS(NS,'path');
  e.setAttribute('d','M'+p[0]+' '+p[1]+'A'+r+' '+r+' 0 '+la+' 1 '+q[0]+' '+q[1]);e.setAttribute('class',cls);return e}
 var zg=$('zones');
 zg.appendChild(arc(104,ang(0),ang(4000),'zone zg'));
 zg.appendChild(arc(104,ang(4000),ang(5500),'zone zo'));
 zg.appendChild(arc(104,ang(5500),ang(7000),'zone zr'));
 var tg=$('ticks');
 for(var v=0;v<=RMAX;v+=500){var a=ang(v),maj=(v%1000===0),o=pol(96,a),i=pol(maj?84:90,a),
  ln=document.createElementNS(NS,'line');
  ln.setAttribute('x1',o[0]);ln.setAttribute('y1',o[1]);ln.setAttribute('x2',i[0]);ln.setAttribute('y2',i[1]);
  ln.setAttribute('class',maj?'tickM':'tick');tg.appendChild(ln);
  if(maj){var l=pol(72,a),t=document.createElementNS(NS,'text');
   t.setAttribute('x',l[0]);t.setAttribute('y',l[1]);t.setAttribute('class','tlbl');t.textContent=v/1000;tg.appendChild(t)}}
 return {set:function(rpm){
  $('needle').style.transform='rotate('+ang(rpm)+'deg)';
  $('rv').textContent=rpm;
  $('rv').style.fill=rpm>=5500?'var(--r)':(rpm>=4000?'var(--o)':'var(--ink)');
 }};
})();
async function tick(){try{
 var d=await(await fetch('/data',{cache:'no-store'})).json();
 $('stat').textContent='connecte · '+(d.fw||'');
 TACH.set(d.rpm);
 var t=$('temp');
 if(d.tempValid){t.firstChild.textContent=Math.round(d.temp);col(t,d.temp>=105?'r':d.temp>=95?'o':'g')}
 else{t.firstChild.textContent='--';col(t,'r')}
 $('tmax').textContent='max '+(d.tMax>-100?Math.round(d.tMax)+'°C':'—');
 var v=$('volt');
 if(d.voltValid){v.firstChild.textContent=d.volt.toFixed(1);col(v,(d.volt<11.8||d.volt>15)?'r':(d.volt<12.4||d.volt>14.8)?'o':'g')}
 else{v.firstChild.textContent='--';col(v,'r')}
 $('vmm').textContent=(d.vMin<900?d.vMin.toFixed(1):'—')+' / '+(d.vMax>-100?d.vMax.toFixed(1):'—')+' V';
 $('al').classList.toggle('on',!!d.alarm);
}catch(e){$('stat').textContent='deconnecte'}}
setInterval(tick,250);tick();
async function loadCfg(){try{
 var c=await(await fetch('/config',{cache:'no-store'})).json();
 $('cTempOk').value=c.tempOk;$('cTempWn').value=c.tempWarn;
 $('cVLo').value=c.voltLo;$('cVHi').value=c.voltHi;
 $('cVCal').value=c.voltCal;$('cBright').value=c.bright;$('bv').textContent=c.bright;
}catch(e){}}
$('cBright').addEventListener('input',function(){$('bv').textContent=this.value});
$('save').addEventListener('click',async function(){
 localStorage.setItem('tok',$('tok').value);
 var q='/set?tempOk='+$('cTempOk').value+'&tempWarn='+$('cTempWn').value+'&voltLo='+$('cVLo').value+'&voltHi='+$('cVHi').value+'&voltCal='+$('cVCal').value+'&bright='+$('cBright').value+'&token='+encodeURIComponent($('tok').value);
 try{var r=await fetch(q);$('msg').textContent=r.ok?'Enregistre OK':(r.status==403?'Mot de passe refuse':'Echec');}catch(e){$('msg').textContent='Echec';}
 setTimeout(function(){$('msg').textContent=''},2500);
});
$('rst').addEventListener('click',async function(){
 localStorage.setItem('tok',$('tok').value);
 try{var r=await fetch('/reset?token='+encodeURIComponent($('tok').value));$('msg').textContent=r.ok?'Min/max remis a zero':(r.status==403?'Mot de passe refuse':'Echec');}catch(e){$('msg').textContent='Echec';}
 setTimeout(function(){$('msg').textContent=''},2500);
});
$('tok').value=localStorage.getItem('tok')||'';
loadCfg();
$('fwbtn').addEventListener('click',function(){
 var f=$('fw').files[0];
 if(!f){$('fwmsg').textContent='Choisis un fichier .bin';return;}
 localStorage.setItem('tok',$('tok').value);
 var fd=new FormData();fd.append('update',f,f.name);
 var x=new XMLHttpRequest();
 x.open('POST','/update?token='+encodeURIComponent($('tok').value));
 x.upload.onprogress=function(e){if(e.lengthComputable)$('fwmsg').textContent='Envoi '+Math.round(e.loaded/e.total*100)+'%';};
 x.onload=function(){$('fwmsg').textContent=(x.status==200)?x.responseText:(x.status==403?'Mot de passe refuse':'Echec '+x.status);};
 x.onerror=function(){$('fwmsg').textContent='Connexion perdue (reboot probable = flash OK)';};
 $('fwmsg').textContent='Envoi 0%';
 x.send(fd);
});
$('scrbtn').addEventListener('click',function(){
 $('fwmsg').textContent='Re-init ecran...';
 fetch('/screen').then(function(r){$('fwmsg').textContent=r.ok?'Ecran reinitialise':'Echec';}).catch(function(){$('fwmsg').textContent='Echec';});
});
$('rebootbtn').addEventListener('click',function(){
 if(!confirm('Redemarrer le cadran maintenant ?'))return;
 localStorage.setItem('tok',$('tok').value);
 $('fwmsg').textContent='Redemarrage...';
 fetch('/reboot?token='+encodeURIComponent($('tok').value)).then(function(r){
  $('fwmsg').textContent=r.ok?'Redemarrage en cours...':(r.status==403?'Mot de passe refuse':'Echec '+r.status);
 }).catch(function(){$('fwmsg').textContent='Redemarrage... (connexion perdue = normal)';});
});
async function loadLog(){try{
 var t=await(await fetch('/log',{cache:'no-store'})).text();
 var el=$('log');var atBottom=el.scrollTop+el.clientHeight>=el.scrollHeight-8;
 el.textContent=t;
 if(atBottom)el.scrollTop=el.scrollHeight;
}catch(e){}}
setInterval(loadLog,2000);loadLog();
// ---- Anti-veille ecran : Wake Lock API si dispo (HTTPS), sinon video muette en boucle (HTTP/AP) ----
(function(){
 var armed=false,wl=null,vid=null;
 var MP4="data:video/mp4;base64,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";
 var WEBM="data:video/webm;base64,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";
 function mkVid(){
  vid=document.createElement('video');
  vid.muted=true;vid.setAttribute('muted','');vid.setAttribute('playsinline','');
  vid.setAttribute('webkit-playsinline','');vid.setAttribute('loop','');
  vid.style.cssText='position:fixed;left:-2px;top:-2px;width:1px;height:1px;opacity:0;pointer-events:none';
  var a=document.createElement('source');a.src=WEBM;a.type='video/webm';
  var b=document.createElement('source');b.src=MP4;b.type='video/mp4';
  vid.appendChild(a);vid.appendChild(b);document.body.appendChild(vid);
 }
 async function reqLock(){if('wakeLock'in navigator){try{wl=await navigator.wakeLock.request('screen');}catch(e){}}}
 async function arm(){await reqLock();if(!vid)mkVid();try{await vid.play();}catch(e){}armed=true;render();}
 function disarm(){armed=false;if(wl){try{wl.release();}catch(e){}wl=null;}if(vid){try{vid.pause();}catch(e){}}render();}
 function render(){var b=$('wake');b.textContent=armed?"Ecran maintenu allume":"Garder l'ecran allume";b.classList.toggle('on',armed);}
 $('wake').addEventListener('click',function(){armed?disarm():arm();});
 document.addEventListener('visibilitychange',function(){
  if(document.visibilityState==='visible'&&armed){reqLock();if(vid)vid.play().catch(function(){});}
 });
 render();
})();
// ---- Mode plein ecran cadran : Fullscreen API (Android) + mise en page focalisee (fallback iOS) ----
(function(){
 var on=false;
 function fsEl(){return document.fullscreenElement||document.webkitFullscreenElement;}
 function apply(v){on=v;document.body.classList.toggle('fs',v);$('fs').textContent=v?"Quitter le plein ecran":"Plein ecran";}
 $('fs').addEventListener('click',function(){
  if(!on){
   apply(true);
   var e=document.documentElement,rf=e.requestFullscreen||e.webkitRequestFullscreen;
   if(rf){try{rf.call(e);}catch(_){}}
   if(screen.orientation&&screen.orientation.lock){try{screen.orientation.lock('landscape').catch(function(){});}catch(_){}}
  }else{
   apply(false);
   var xf=document.exitFullscreen||document.webkitExitFullscreen;
   if(xf&&fsEl()){try{xf.call(document);}catch(_){}}
  }
 });
 function sync(){if(!fsEl()&&on)apply(false);}
 document.addEventListener('fullscreenchange',sync);
 document.addEventListener('webkitfullscreenchange',sync);
})();
</script></body></html>
)HTML";

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
</style></head><body>
<header><span>RENAULT 4L &middot; TELEMETRIE</span><span id="stat">...</span></header>
<main>
<div class="alarm" id="al">&#9888; ALARME</div>
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
</script></body></html>
)HTML";

const fs = require('node:fs/promises');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { randomUUID } = require('node:crypto');
const { EventEmitter } = require('node:events');
const { editorExecutable } = require('./editor-runtime.cjs');

function layout(value) {
  const result = {};
  for (const name of ['x','y','width','height','viewportWidth','viewportHeight']) {
    if (!Number.isFinite(value?.[name]) || value[name] < 0 || value[name] > 20000) throw new Error('Área do editor inválida.');
    result[name] = value[name];
  }
  if (!result.viewportWidth || !result.viewportHeight) throw new Error('Janela sem dimensões.');
  result.visible = value.visible === true;
  return result;
}
function parseState(text){
  let group,groups={};for(const line of text.split(/\r?\n/)){
    const heading=/^\[(.+)\]$/.exec(line);if(heading){group=groups[heading[1]]={};continue;}
    const item=/^([^=]+)=(.*)$/.exec(line);if(item&&group)group[item[1]]=item[2];
  }
  return {host:'native',windows:Object.keys(groups).filter(k=>/^window\d+$/.test(k)).map(k=>({parent:groups[k].attached==='true'?'native':'none',visible:groups[k].visible==='true',width:Number(groups[k].width),height:Number(groups[k].height),requestedWidth:Number(groups[k].requestedWidth),requestedHeight:Number(groups[k].requestedHeight),geometryUpdates:Number(groups[k].geometryUpdates),layered:groups[k].layered==='true',x:Number(groups[k].x),y:Number(groups[k].y),requestedX:Number(groups[k].requestedX),requestedY:Number(groups[k].requestedY),tab:Number(groups[k].tab)}))};
}
class NativeWorkspace extends EventEmitter {
  constructor(root, window, profile) { super();this.root=root;this.window=window;this.profile=profile||path.join(root,'profile');this.child=null;this.inbox=null;this.currentLayout=null;this.lastLayout=null;this.layoutQueue=Promise.resolve();this.attached=0;this.poll=null; }
  async command(extension,text='') {if(!this.inbox)return;const temporary=path.join(this.inbox,randomUUID()+'.tmp');await fs.writeFile(temporary,text,'utf8');await fs.rename(temporary,temporary.replace(/\.tmp$/,extension));}
  async inspect(){if(!this.inbox)return {host:'native',windows:[]};return parseState(await fs.readFile(path.join(this.inbox,'state.ini'),'utf8'));}
  async open(filename) {
    if (process.platform !== 'win32') throw new Error('O editor integrado está disponível nesta versão para Windows.');
    if (filename) { filename=await fs.realpath(filename);if(!['.xopp','.pdf'].includes(path.extname(filename).toLowerCase()))throw new Error('Escolha uma anotação ou um PDF.'); }
    if(this.child){await this.command('.open',filename||'');return true;}
    const executable=process.env.XOURNAL_WORKSPACE_EDITOR || await editorExecutable(this.root);
    const relative=path.relative(this.root,executable);if(relative.startsWith('..')||path.isAbsolute(relative)||path.basename(executable).toLowerCase()!=='xournalpp.exe')throw new Error('Editor fora do laboratório.');
    this.inbox=await fs.mkdtemp(path.join(await this.temp(),'workspace-'));this.lastLayout=null;
    await this.setLayout(this.currentLayout||{x:0,y:0,width:1,height:1,viewportWidth:1,viewportHeight:1,visible:false});this.attached=0;
    const handle=this.window.getNativeWindowHandle();
    const env={...process.env,XOURNAL_LAB_ROOT:this.root,XOURNAL_WORKSPACE_INBOX:this.inbox,XOURNAL_WORKSPACE_HOST:(handle.length===8?handle.readBigUInt64LE():BigInt(handle.readUInt32LE())).toString(),XOURNAL_WORKSPACE_OWNER:String(process.pid),XOURNAL_TRACOS_WORKER:path.join(this.root,'tools/tracos-engine/TracosRecognitionHost.exe')};
    for(const [key,folder] of Object.entries({XDG_CONFIG_HOME:'config',XDG_CACHE_HOME:'cache',XDG_DATA_HOME:'data',XDG_STATE_HOME:'state',TEMP:'temp',TMP:'temp'})) {env[key]=path.join(this.profile,folder);await fs.mkdir(env[key],{recursive:true});}
    env.PATH=path.join(this.root,'tools/TinyTeX/bin/windows')+path.delimiter+(env.PATH||env.Path||'');
    const log=await fs.open(path.join(this.inbox,'native.log'),'a');
    const child=spawn(executable,filename?[filename]:[],{env,cwd:path.join(this.root,'documents'),stdio:['ignore',log.fd,log.fd],shell:false});this.child=child;
    await new Promise((resolve,reject)=>{child.once('spawn',resolve);child.once('error',reject);}).catch(error=>{this.child=null;throw error;}).finally(()=>log.close());
    this.poll=setInterval(async()=>{try{const state=await this.inspect();if(state.windows.length>this.attached){this.attached=state.windows.length;this.emit('attached',state);} }catch{}},160);this.poll.unref();
    child.once('exit',(code,signal)=>{if(this.child!==child)return;this.child=null;clearInterval(this.poll);if(code)this.emit('failure','O editor encerrou com erro '+code+'. Consulte o log do laboratório.');this.emit('closed');});
    await new Promise((resolve,reject)=>{
      const done=()=>{cleanup();resolve();};
      const failed=()=>{cleanup();reject(new Error('O editor encerrou antes de abrir o arquivo.'));};
      const timer=setTimeout(()=>{cleanup();reject(new Error('O editor demorou a iniciar. Feche os avisos que estiverem abertos e tente novamente.'));},25000);
      const cleanup=()=>{clearTimeout(timer);this.removeListener('attached',done);this.removeListener('closed',failed);};
      this.once('attached',done);this.once('closed',failed);
    });
    this.emit('opened');return true;
  }
  async temp(){const dir=path.join(this.profile,'temp');await fs.mkdir(dir,{recursive:true});return dir;}
  async writeLayout(){
    if(!this.inbox)return;const value=this.currentLayout||{x:0,y:0,width:1,height:1,viewportWidth:1,viewportHeight:1,visible:false};
    const text='[layout]\n'+Object.entries(value).map(([key,value])=>key+'='+value).join('\n')+'\n';
    if(text===this.lastLayout)return;
    const temporary=path.join(this.inbox,randomUUID()+'.layout');await fs.writeFile(temporary,text);for(let attempt=0;;attempt++){try{await fs.rename(temporary,path.join(this.inbox,'layout.ini'));this.lastLayout=text;break;}catch(error){if(!['EPERM','EACCES','EBUSY'].includes(error.code)||attempt>=6)throw error;await new Promise(r=>setTimeout(r,15*(attempt+1)));}}
  }
  setLayout(value){this.currentLayout=layout(value);this.layoutQueue=this.layoutQueue.catch(()=>{}).then(()=>this.writeLayout());return this.layoutQueue.then(()=>true);}
  close(){this.command('.close').catch(error=>this.emit('failure',error.message));}
}
module.exports={NativeWorkspace,layout,parseState};

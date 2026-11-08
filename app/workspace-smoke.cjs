const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
function event(source,name,timeout=20000){return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{source.removeListener(name,done);reject(new Error('Timeout: '+name));},timeout);function done(value){clearTimeout(timer);resolve(value);}source.once(name,done);});}
module.exports=async ({window,workspace,app})=>{
  const folder=process.env.XOURNAL_WORKSPACE_QA;
  if(!folder)throw new Error('Use um perfil descartável para o ensaio.');
  const report={};
  const inspect=()=>workspace.inspect();
  try{
    await window.webContents.executeJavaScript('window.panelReady');
    let waiting=event(workspace,'attached');await workspace.open(path.join(folder,'Aba-A.xopp'));await waiting;
    window.webContents.send('workspace:event',{type:'show'});await delay(1400);
    let state=await inspect();report.initialState=state;assert.equal(state.windows.length,1);assert.equal(state.windows[0].parent,state.host);assert.ok(state.windows[0].visible);assert.ok(state.windows[0].width>300);report.attached=true;
    const chrome=await window.webContents.executeJavaScript(`({
      noTopChrome: !document.querySelector('.topbar,.workspace-tabs'),
      sidebarNavigation: !!document.querySelector('.sidebar #workspace-home') && !!document.querySelector('.sidebar #workspace-editor'),
      top: document.querySelector('#editor-surface').getBoundingClientRect().top,
      active: document.querySelector('#workspace-editor').classList.contains('active')
    })`);
    assert.ok(chrome.noTopChrome&&chrome.sidebarNavigation&&chrome.active);assert.equal(chrome.top,8);report.sidebarLayout=true;
    await window.webContents.executeJavaScript("document.querySelector('#workspace-home').click()");await delay(250);state=await inspect();assert.ok(state.windows.every(w=>!w.visible));report.homeHidesEditor=true;
    await window.webContents.executeJavaScript("document.querySelector('#workspace-editor').click()");await delay(250);state=await inspect();assert.equal(state.windows.filter(w=>w.visible).length,1);report.editorReturns=true;
    waiting=event(workspace,'attached');await workspace.open(path.join(folder,'Aba-B.xopp'));await waiting;await delay(500);state=await inspect();assert.equal(state.windows.length,2);assert.equal(state.windows.filter(w=>w.visible).length,1);assert.ok(state.windows.every(w=>w.parent===state.host));report.secondDocument=true;
    const checkPosition=async()=>{
      await delay(500);const result=await inspect();const selected=result.windows.filter(w=>w.visible);assert.equal(selected.length,1);
      const active=selected[0];assert.ok(Math.abs(active.x-active.requestedX)<=2&&Math.abs(active.y-active.requestedY)<=2,JSON.stringify(active));return result;
    };
    for(let i=0;i<3;++i){waiting=event(workspace,'attached');await workspace.open();await waiting;await checkPosition();}
    report.fiveTabsAligned=true;
    for(let i=0;i<10;++i){await workspace.command('.qa','next');await checkPosition();}
    report.tabSwitchesAligned=true;
    for(let i=0;i<3;++i){await workspace.command('.qa','close');await checkPosition();}
    report.tabCloseAligned=true;
    state=await inspect();
    const beforeNav=state.windows.find(w=>w.visible).width;
    await window.webContents.executeJavaScript("document.querySelector('#toggle-nav').click()");await delay(760);
    state=await inspect();assert.notEqual(state.windows.find(w=>w.visible).width,beforeNav);report.sidebarResize=true;
    await window.webContents.executeJavaScript("document.querySelector('#toggle-nav').click()");await delay(760);
    state=await inspect();
    window.unmaximize();window.setFullScreen(false);window.setSize(1300,850);await delay(700);state=await inspect();
    const before=state.windows.find(w=>w.visible).width;window.setSize(950,720);await delay(600);state=await inspect();assert.ok(state.windows.find(w=>w.visible).width<before);report.resize=true;
    for(let i=0;i<18;++i){window.setSize(i%2?1250:950,i%3?760:900);await delay(60);}
    window.maximize();await delay(500);window.unmaximize();await delay(500);window.setSize(1100,800);await delay(900);
    state=await inspect();let active=state.windows.find(w=>w.visible);
    assert.ok(Math.abs(active.width-active.requestedWidth)<=2,JSON.stringify(active));
    assert.ok(Math.abs(active.height-active.requestedHeight)<=2,JSON.stringify(active));
    // GTK on Windows may legitimately use a layered system visual. The regression
    // is an invalid backing-surface update, checked in the native log below.
    report.nativeLayeredWindow=active.layered;
    const updates=active.geometryUpdates;await delay(600);state=await inspect();active=state.windows.find(w=>w.visible);
    assert.equal(active.geometryUpdates,updates);report.stableGeometry=true;
    const loadingHidden=await window.webContents.executeJavaScript("document.querySelector('#editor-loading').hidden");
    assert.ok(loadingHidden);report.loadingDismissed=true;
    waiting=event(workspace,'closed');workspace.close();await waiting;report.cleanShutdown=true;
    const nativeLog=await fs.readFile(path.join(workspace.inbox,'native.log'),'utf8');
    assert.ok(!nativeLog.includes('UpdateLayeredWindow failed'),nativeLog.slice(-1500));report.resizeStressWithoutLayeredErrors=true;
    await fs.writeFile(path.join(folder,'workspace-smoke-result.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));app.quit();
  }catch(error){report.error=error.message;await fs.writeFile(path.join(folder,'workspace-smoke-result.json'),JSON.stringify(report,null,2));workspace.close();console.error(error);setTimeout(()=>app.exit(1),1500);}
};

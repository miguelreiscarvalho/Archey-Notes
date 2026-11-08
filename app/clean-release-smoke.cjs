const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
const {Library}=require('./library.cjs');
const {runWorkflow}=require('./backend.cjs');
const delay=ms=>new Promise(r=>setTimeout(r,ms));

module.exports=async({window,workspace,app,root})=>{
  const qa=process.env.XOURNAL_WORKSPACE_QA;
  assert.ok(qa,'A disposable QA directory is required');
  try{
    await window.webContents.executeJavaScript('window.panelReady');
    const catalog=await new Library(root).snapshot();
    assert.deepEqual(catalog,{version:1,subjects:[]});
    assert.deepEqual(await fs.readdir(path.join(root,'documents/Aulas')),[]);
    const models=(await fs.readdir(path.join(root,'documents/Modelos'))).filter(n=>n.endsWith('.xopp'));
    assert.equal(models.length,3);
    const ui=await window.webContents.executeJavaScript(`({
      ready:!!document.querySelector('#workspace-home'),
      brand:document.querySelector('.brand-icon img').complete,
      errors:document.querySelector('#status').classList.contains('error')
    })`);
    assert.ok(ui.ready&&ui.brand&&!ui.errors,JSON.stringify(ui));
    await window.webContents.executeJavaScript('new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)))');
    await delay(500);
    await fs.writeFile(path.join(qa,'clean-home.png'),(await window.webContents.capturePage()).toPNG());
    const filename=await runWorkflow(root,['new','--course=Teste-publicacao','--title=Pagina vazia',
      '--date=2026-11-08','--template=Quadriculado']);
    assert.ok(filename.startsWith(path.join(root,'documents/Aulas')));
    await workspace.open(filename);
    window.webContents.send('workspace:event',{type:'show'});
    let state;
    for(let i=0;i<100;i++){
      await delay(100);state=await workspace.inspect();
      if(state.windows.some(w=>w.visible&&w.width>300))break;
    }
    assert.equal(state.windows.length,1);
    assert.ok(state.windows[0].visible&&state.windows[0].width>300,JSON.stringify(state));
    const result={emptyLibrary:true,emptySubjectsDirectory:true,blankModels:models.length,
      interfaceReady:true,portablePythonCreatesNote:true,nativeDocumentOpens:true,
      rootUsesDeveloperLab:false};
    await fs.writeFile(path.join(qa,'clean-release-result.json'),JSON.stringify(result,null,2));
    console.log(JSON.stringify(result));workspace.close();await delay(1000);app.quit();
  }catch(error){console.error(error);workspace.close();await delay(500);app.exit(1);}
};

const { app, BrowserWindow, ipcMain, dialog, shell, nativeImage } = require('electron');
const fs = require('node:fs/promises');
const path = require('node:path');
const { fileURLToPath } = require('node:url');
const { spawn } = require('node:child_process');
const { validateLesson, lessonArgs, runWorkflow } = require('./backend.cjs');
const { Library } = require('./library.cjs');
const { NativeWorkspace } = require('./native-workspace.cjs');

const root = path.resolve(process.env.XOURNAL_LAB_ROOT || path.join(__dirname, '../../..'));
const library = new Library(root);
const page = path.join(__dirname, 'renderer/index.html');
app.setAppUserModelId('com.xournal.lab.archey');
let window;
let busy = false;
let models = [];
const approvedPdfs = new Set();
const outputs = new Set();
const createdDocuments = new Set();

// Keep Electron's own state inside the lab, independent from Xournal++ and other Electron apps.
const profile = process.env.XOURNAL_STUDY_PROFILE || path.join(root, 'profile');
app.setPath('userData', path.join(profile, 'study-ui'));
app.setPath('sessionData', path.join(profile, 'study-ui/session'));
if (!app.requestSingleInstanceLock()) app.quit();
else {
  app.on('second-instance', () => { if (window) { if (window.isMinimized()) window.restore(); window.show(); window.focus(); window.webContents.send('workspace:event',{type:'home'}); } });
  app.whenReady().then(createWindow).catch(error => { console.error(error); app.exit(1); });
}
app.on('window-all-closed', () => app.quit());

function validSender(event) {
  try { return event.sender === window.webContents && event.senderFrame === window.webContents.mainFrame && path.resolve(fileURLToPath(event.senderFrame.url)) === path.resolve(page); }
  catch { return false; }
}

function handle(name, callback, exclusive = false) {
  ipcMain.handle(name, async (event, input) => {
    if (!validSender(event)) return { ok: false, error: 'Origem da operação não autorizada.' };
    if (exclusive && busy) return { ok: false, error: 'Aguarde a operação atual.' };
    if (exclusive) busy = true;
    try { return { ok: true, data: await callback(input) }; }
    catch (error) { return { ok: false, error: error.message }; }
    finally { if (exclusive) busy = false; }
  });
}

async function chooseDocument() {
  const result = await dialog.showOpenDialog(window, { title: 'Escolha a versão salva da anotação', defaultPath: path.join(root, 'documents'), properties: ['openFile'], filters: [{ name: 'Xournal++', extensions: ['xopp'] }] });
  return result.canceled ? null : result.filePaths[0];
}

let workspace;
let closingWindow = false;
async function openInXournal(output) {
  await workspace.open(output);
  window.webContents.send('workspace:event', {type:'show'});
  return true;
}

async function createWindow() {
  models = (await fs.readdir(path.join(root, 'documents/Modelos'))).filter(name => name.endsWith('.xopp')).map(name => name.slice(0, -5));
  window = new BrowserWindow({
    width: 1150, height: 830, minWidth: 780, minHeight: 650,
    title: 'Xournal Lab — Estudos', icon: path.join(__dirname, 'renderer/assets/brand/archey-notes.ico'), backgroundColor: '#eef0f4', show: false,
    autoHideMenuBar: true,
    webPreferences: { backgroundThrottling: false, preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true, nodeIntegration: false, sandbox: true, webSecurity: true }
  });
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.webContents.on('will-navigate', event => event.preventDefault());
  window.webContents.session.setPermissionRequestHandler((_contents, _permission, callback) => callback(false));
  workspace = new NativeWorkspace(root, window, profile);
  workspace.on('closed', () => {
    window.webContents.send('workspace:event', {type:'closed'});
    if (closingWindow) window.destroy();
  });
  workspace.on('error', event => window.webContents.send('workspace:event', {type:'error',message:event.message}));
  workspace.on('failure', message => window.webContents.send('workspace:event', {type:'error',message}));
  window.on('close', event => {
    if (busy) { event.preventDefault(); window.focus(); return; }
    if (workspace.child) {
      event.preventDefault();closingWindow=true;workspace.close();
      setTimeout(()=>{closingWindow=false;},5000).unref();
    }
  });
  handle('workspace:layout', value => workspace.setLayout(value));
  handle('workspace:new', () => openInXournal(null), true);
  handle('workspace:close', () => {workspace.close();return true;});
  handle('workspace:open', async () => {
    const chosen=await dialog.showOpenDialog(window,{title:'Abrir no espaço de escrita',defaultPath:path.join(root,'documents'),properties:['openFile'],filters:[{name:'Anotações e PDF',extensions:['xopp','pdf']}]});
    return chosen.canceled ? false : openInXournal(chosen.filePaths[0]);
  }, true);
  handle('study:bootstrap', async () => ({ models, version: '1.3.8' }));
  handle('study:choose-pdf', async () => {
    const result = await dialog.showOpenDialog(window, { title: 'Material da aula', properties: ['openFile'], filters: [{ name: 'PDF', extensions: ['pdf'] }] });
    if (result.canceled) return null;
    const selected = result.filePaths[0]; approvedPdfs.add(selected);
    return { path: selected, name: path.basename(selected) };
  }, true);
  handle('study:new', async input => {
    const values = validateLesson(input, models);
    await library.checkTarget(values.course, input.chapterId);
    const pdf = input.pdf || null;
    if (pdf && !approvedPdfs.has(pdf)) throw new Error('Selecione novamente o PDF pelo botão da interface.');
    const output = await runWorkflow(root, lessonArgs(values, pdf));
    outputs.add(output); createdDocuments.add(output);
    let warning;
    try { await library.registerLesson(output, values.title, values.course, input.chapterId); }
    catch { warning = 'A aula foi criada, mas não foi possível atualizar a biblioteca. Atualize Matérias para localizá-la.'; }
    return { path: output, name: path.basename(output), warning };
  }, true);
  for (const operation of ['export', 'backup', 'pack']) {
    handle('study:' + operation, async () => {
      const source = await chooseDocument();
      if (!source) return null;
      const args = [operation, source];
      if (operation === 'pack') {
        const result = await dialog.showOpenDialog(window, { title: 'Destino do pacote — pode ser uma pasta OneDrive', properties: ['openDirectory', 'createDirectory'] });
        if (result.canceled) return null;
        args.push(result.filePaths[0]);
      }
      const output = await runWorkflow(root, args); outputs.add(output);
      return { path: output, name: path.basename(output) };
    }, true);
  }
  handle('study:show-output', async output => {
    if (!outputs.has(output)) throw new Error('Arquivo não pertence a esta sessão.');
    shell.showItemInFolder(output); return true;
  });
  handle('study:open-document', async output => {
    if (!createdDocuments.has(output)) throw new Error('Anotação não pertence a esta sessão.');
    return openInXournal(output);
  }, true);
  handle('library:list', () => library.snapshot(), true);
  handle('library:subject', input => library.createSubject(input.name), true);
  handle('library:chapter', input => library.createChapter(input.subjectId, input.name), true);
  handle('library:rename', input => library.rename(input.type, input.id, input.name), true);
  handle('library:order', input => library.reorder(input.type, input.id, input.delta), true);
  handle('library:move', input => library.move(input.documentId, input.chapterId), true);
  handle('library:link', async input => {
    const result = await dialog.showOpenDialog(window, { title: 'Adicionar anotações ao capítulo', defaultPath: path.join(root, 'documents'), properties: ['openFile', 'multiSelections'], filters: [{ name: 'Xournal++', extensions: ['xopp'] }] });
    return result.canceled ? null : library.linkDocuments(input.chapterId, result.filePaths);
  }, true);
  handle('library:open', async id => openInXournal(await library.documentPath(id)), true);
  handle('library:show', async id => { shell.showItemInFolder(await library.documentPath(id)); return true; }, true);
  handle('library:relink', async id => {
    const filename = await chooseDocument();
    if (!filename) return false;
    await library.relink(id, filename); return true;
  }, true);
  window.once('ready-to-show', () => window.show());
  await window.loadFile(page);
  if (process.argv.includes('--clean-release-smoke-test') && process.env.XOURNAL_WORKSPACE_QA) {
    await require('./clean-release-smoke.cjs')({window,workspace,app,root}); return;
  }
  if (process.argv.includes('--sidebar-motion-test')) { await require('./sidebar-motion-smoke.cjs')({window,app}); return; }
  if (process.argv.includes('--workspace-preview') && process.env.XOURNAL_WORKSPACE_QA) {
    await openInXournal(path.join(process.env.XOURNAL_WORKSPACE_QA, 'Aba-A.xopp'));
  }
  if (process.argv.includes('--workspace-smoke-test')) {
    await require('./workspace-smoke.cjs')({window,workspace,app});
    return;
  }
  if (process.argv.includes('--smoke-test')) {
    const status = await window.webContents.executeJavaScript(`Promise.resolve(window.panelReady).then(async () => {
      const initialTheme = document.documentElement.dataset.theme;
      document.querySelector('#theme').click();
      const initialNav = document.documentElement.dataset.nav;
      document.querySelector('#toggle-nav').click();
      const navigationChanged = document.documentElement.dataset.nav !== initialNav;
      const navigationAccessible = document.querySelector('#toggle-nav').getAttribute('aria-expanded') === String(document.documentElement.dataset.nav !== 'collapsed');
      document.querySelector('#toggle-nav').click();
      const assetsReady = [...document.querySelectorAll('.sidebar img')].every(image => image.complete && image.naturalWidth > 0);
      const themeChanged = document.documentElement.dataset.theme !== initialTheme;
      document.querySelector('#theme').click();
      const sections = ['export','backup','pack'].every(view => { document.querySelector('[data-view="'+view+'"]').click(); return !document.querySelector('#view-action').hidden && document.querySelector('#view-new').hidden; });
      const actionAssets = [];
      for (const view of ['export','backup','pack']) {
        document.querySelector('[data-view="'+view+'"]').click();
        const image = document.querySelector('#action-icon img');
        if (!image) { actionAssets.push(false); continue; }
        try { await image.decode(); actionAssets.push(image.naturalWidth > 0); }
        catch { actionAssets.push(false); }
      }
      document.querySelector('[data-view="new"]').click();
      const libraryReady = !!document.querySelector('#view-library') && typeof window.study.libraryList === 'function';
      return { title: document.title, models: document.querySelectorAll('#template option').length, bridge: typeof window.study.createLesson, node: typeof window.require, form: !!document.querySelector('#lesson-form'), themeChanged, navigationChanged, navigationAccessible, assetsReady, actionAssets: actionAssets.every(Boolean), sections, libraryReady };
    })`);
    status.applicationIconLoaded = !nativeImage.createFromPath(path.join(__dirname,'renderer/assets/brand/archey-notes.ico')).isEmpty();
    if (!status.applicationIconLoaded || status.models < 4 || status.bridge !== 'function' || status.node !== 'undefined' || !status.form || !status.themeChanged || !status.navigationChanged || !status.navigationAccessible || !status.assetsReady || !status.actionAssets || !status.sections || !status.libraryReady) throw new Error(JSON.stringify(status));
    await fs.writeFile(path.join(__dirname, 'electron-smoke-result.json'), JSON.stringify(status, null, 2));
    console.log(JSON.stringify(status)); app.quit();
  }
}

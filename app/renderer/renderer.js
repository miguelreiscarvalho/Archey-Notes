const $ = selector => document.querySelector(selector);
const views = {
  library: { heading: 'Archey Notes', subtitle: '' },
  new: { heading: 'Nova aula', subtitle: '' },
  export: { heading: 'Exportar PDF', subtitle: '', title: 'Exportar anotação', description: 'Selecione um arquivo .xopp salvo. O PDF será criado com data e hora no nome.', detail: 'Um backup é criado antes da exportação.', icon: '↗' },
  backup: { heading: 'Guardar versões', subtitle: '', title: 'Criar backup', description: 'Crie um pacote da anotação salva e dos arquivos associados, com data e hora.', detail: 'Anotações com áudio ainda não são suportadas.', icon: '↶' },
  pack: { heading: 'Compartilhar', subtitle: '', title: 'Criar pacote', description: 'Selecione a anotação e a pasta de destino do pacote.', detail: 'Extraia o ZIP inteiro antes de abrir. O envio para a nuvem depende do aplicativo de sincronização.', icon: '□' }
};
let currentView = 'library';
let pdf = null;
let resultPath = null;
let working = false;

function setView(name) {
  if (working) return;
  currentView = name;
  const info = views[name];
  $('#heading').replaceChildren(...info.heading.split('\n').flatMap((line, index) => index ? [document.createElement('br'), document.createTextNode(line)] : [document.createTextNode(line)]));
  $('#subtitle').textContent = info.subtitle;
  $('#subtitle').hidden = !info.subtitle;
  $('#view-new').hidden = name !== 'new'; $('#view-action').hidden = name === 'new';
  $('#view-library').hidden = name !== 'library';
  $('#view-action').hidden = ['new','library'].includes(name);
  $('#result').hidden = name==='library' || !resultPath;
  document.querySelectorAll('[data-view]').forEach(button => { const active = button.dataset.view === name; button.classList.toggle('active', active); if (active) button.setAttribute('aria-current','page'); else button.removeAttribute('aria-current'); });
  if (!['new','library'].includes(name)) {
    $('#action-title').textContent = info.title; $('#action-description').textContent = info.description;
    $('#action-detail').textContent = info.detail;
    const image = document.createElement('img'); image.alt = '';
    image.src = `assets/studio/${{ export:'export', backup:'backup', pack:'share' }[name]}.svg`;
    $('#action-icon').replaceChildren(image);
  }
  $('#status').textContent = name === 'new' ? '' : name==='library' ? '' : 'Selecione uma anotação salva.';
  $('#status').classList.remove('error');
}

function preview() {
  $('#preview-course').textContent = $('#course').value.trim().toUpperCase() || 'DISCIPLINA';
  $('#preview-title').textContent = $('#subject').value.trim() || 'Título da aula';
  $('#preview-date').textContent = $('#date').value.split('-').reverse().join('/');
  $('#preview-model').textContent = pdf ? 'Material em PDF' : $('#template').value;
}

function unwrap(response) { if (!response.ok) throw new Error(response.error); return response.data; }

async function task(callback, successTitle) {
  if (working) return;
  working = true;
  const disabledBefore = new Map([...document.querySelectorAll('button, input, select')].map(control => [control, control.disabled]));
  for (const control of disabledBefore.keys()) control.disabled = true;
  $('#status').classList.remove('error'); $('#status').textContent = 'Processando…';
  try {
    const result = await callback();
    if (result) {
      resultPath = result.path; $('#result-title').textContent = successTitle;
      $('#result-name').textContent = result.name; $('#result').hidden = false;
      $('#status').textContent = result.warning || 'Operação concluída.';
    } else $('#status').textContent = 'Operação cancelada.';
  } catch (error) { $('#status').textContent = error.message; $('#status').classList.add('error'); }
  finally { working = false; for (const [control, disabled] of disabledBefore) control.disabled = disabled; window.dispatchEvent(new Event('workspace:sync')); }
}

document.querySelectorAll('[data-view]').forEach(button => button.addEventListener('click', () => setView(button.dataset.view)));
['#course','#subject','#date','#template'].forEach(selector => $(selector).addEventListener('input', preview));
$('#choose-pdf').addEventListener('click', async () => {
  await task(async () => { const selected = unwrap(await window.study.choosePdf()); if (selected) { pdf = selected; $('#pdf-name').textContent = selected.name; $('#pdf-chip').hidden = false; $('#template').disabled = true; preview(); } return null; });
  // Restore the model control according to the actual material selection.
  $('#template').disabled = !!pdf;
  if (!$('#status').classList.contains('error')) $('#status').textContent = pdf ? 'PDF selecionado. Uma cópia será usada na aula.' : 'Nenhum PDF selecionado.';
});
$('#remove-pdf').addEventListener('click', () => { pdf = null; $('#pdf-chip').hidden = true; $('#template').disabled = false; preview(); });
$('#lesson-form').addEventListener('submit', event => {
  event.preventDefault();
  const values = { course: $('#course').value, title: $('#subject').value, date: $('#date').value, template: $('#template').value, pdf: pdf?.path || null, chapterId: $('#lesson-chapter').value || null };
  task(async () => {
    const created = unwrap(await window.study.createLesson(values));
    // Record the created file before opening it, so a launcher failure still leaves a visible recovery path.
    resultPath = created.path; $('#result-title').textContent = 'Aula criada'; $('#result-name').textContent = created.name; $('#result').hidden = false;
    unwrap(await window.study.openDocument(created.path));
    try { await window.libraryUi.load(); } catch { created.warning='A aula foi criada. Atualize Matérias para vê-la na biblioteca.'; }
    return created;
  }, 'Aula criada');
});
$('#run-action').addEventListener('click', () => {
  const operations = { export: [window.study.exportSaved, 'PDF exportado'], backup: [window.study.backupSaved, 'Versão guardada'], pack: [window.study.packageSaved, 'Pacote preparado'] };
  const [operation, title] = operations[currentView]; task(async () => unwrap(await operation()), title);
});
$('#show-result').addEventListener('click', async () => { if (!resultPath) return; try { unwrap(await window.study.showOutput(resultPath)); } catch (error) { $('#status').textContent = error.message; $('#status').classList.add('error'); } });
function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  const label = theme === 'dark' ? 'Ativar tema claro' : 'Ativar tema escuro';
  $('#theme').setAttribute('aria-label', label); $('#theme').title = label;
  $('#theme').setAttribute('aria-pressed', String(theme === 'dark'));
  localStorage.setItem('theme', theme);
}
$('#theme').addEventListener('click', () => applyTheme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark'));
applyTheme(localStorage.getItem('theme') || 'light');
function applyNavigation(collapsed) {
  const next = collapsed ? 'collapsed' : 'expanded';
  if (document.documentElement.dataset.nav && document.documentElement.dataset.nav !== next) window.dispatchEvent(new CustomEvent('navigation:motion-start', {detail:{collapsed}}));
  document.documentElement.dataset.nav = next;
  const label = collapsed ? 'Expandir navegação' : 'Recolher navegação';
  $('#toggle-nav').setAttribute('aria-expanded', String(!collapsed));
  $('#toggle-nav').setAttribute('aria-label', label); $('#toggle-nav').title = label;
  localStorage.setItem('navigation-collapsed', String(collapsed));
}
$('#toggle-nav').addEventListener('click', () => applyNavigation(document.documentElement.dataset.nav !== 'collapsed'));
applyNavigation(localStorage.getItem('navigation-collapsed') === 'true');
const now = new Date(); $('#date').value = `${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}-${String(now.getDate()).padStart(2,'0')}`;
window.panelReady = (async () => {
  try {
    const state = unwrap(await window.study.bootstrap());
    $('#template').replaceChildren(...state.models.map(name => { const option = document.createElement('option'); option.value = name; option.textContent = name; return option; }));
    if (state.models.includes('Quadriculado')) $('#template').value = 'Quadriculado';
    $('#version').textContent = state.version; preview();
    setView('library');
    try { await window.libraryUi.load(); } catch(error) { $('#status').textContent=error.message;$('#status').classList.add('error'); }
  } catch (error) { $('#status').textContent = error.message; $('#status').classList.add('error'); $('#create').disabled = true; }
})();

const L = selector => document.querySelector(selector);
let catalog = { subjects: [] };
let selectedSubject = null;
let selectedChapter = null;
let documentSort = 'manual';
const compareNames = new Intl.Collator('pt-BR', { numeric: true, sensitivity: 'base' });
const searchKey = value => value.normalize('NFD').replace(/\p{M}/gu,'').toLocaleLowerCase('pt-BR');
const counted=(n,singular,plural)=>`${n} ${n===1?singular:plural}`;
function element(tag, className, text) {
  const node = document.createElement(tag); if (className) node.className=className;
  if (text !== undefined) node.textContent=text; return node;
}
function button(text, className, callback, accessibleName) {
  const node = element('button',className,text); node.type='button'; node.addEventListener('click',callback);
  if (accessibleName) { node.title=accessibleName; node.setAttribute('aria-label',accessibleName); }
  return node;
}
function subjectNow() { return catalog.subjects.find(s=>s.id===selectedSubject); }
function chapterNow() { return subjectNow()?.chapters.find(c=>c.id===selectedChapter); }
function chapterLabel(subject, chapter) {
  return chapter.default ? chapter.name : `${String(subject.chapters.filter(c=>!c.default).findIndex(c=>c.id===chapter.id)+1).padStart(2,'0')} · ${chapter.name}`;
}
function message(text) { L('#status').textContent=text; L('#status').classList.remove('error'); }

function ask({ title, name, destinations, selectedDestination }) {
  return new Promise(resolve => {
    const dialog=L('#library-dialog'), input=L('#library-name'), destination=L('#library-destination');
    L('#library-dialog-title').textContent=title;
    L('#library-name-field').hidden=destinations!==undefined; input.disabled=destinations!==undefined; input.required=destinations===undefined; input.value=name||'';
    L('#library-destination-field').hidden=destinations===undefined; destination.disabled=destinations===undefined;
    destination.replaceChildren();
    if (destinations) for (const item of destinations) { const option=element('option',null,item.name);option.value=item.id;destination.append(option); }
    if (destinations && selectedDestination) destination.value=selectedDestination;
    L('#library-dialog-cancel').onclick=()=>dialog.close('cancel');
    L('#library-dialog-form').onsubmit=event=>{ event.preventDefault();dialog.close('ok'); };
    dialog.addEventListener('close',()=>resolve(dialog.returnValue==='ok' ? (destinations ? destination.value : input.value.trim()) : null),{once:true});
    dialog.showModal();
    if (destinations) destination.focus(); else input.focus();
  });
}

async function libraryAction(callback, success) {
  let completed=false;
  await task(async()=>{ await callback(); await loadLibrary(); completed=true; return null; });
  if (completed) message(typeof success==='function' ? success() : success);
}
async function loadLibrary() {
  try { catalog=unwrap(await window.study.libraryList()); }
  catch(error) { L('#library-content').replaceChildren(empty('Biblioteca indisponível',error.message));throw error; }
  if (!subjectNow()) { selectedSubject=null; selectedChapter=null; }
  if (subjectNow() && !chapterNow()) selectedChapter=subjectNow().chapters[0]?.id || null;
  L('#subject-options').replaceChildren(...catalog.subjects.map(s=>{const option=element('option');option.value=s.name;return option;}));
  syncChapterOptions(); renderLibrary();
}
function syncChapterOptions(preferred) {
  const select=L('#lesson-chapter'), current=preferred===undefined ? select.value : preferred;
  const subject=catalog.subjects.find(s=>searchKey(s.name.trim())===searchKey(L('#course').value.trim()));
  select.replaceChildren();
  const unfiled=element('option',null,'Sem capítulo definido');unfiled.value='';select.append(unfiled);
  if (subject) subject.chapters.filter(c=>!c.default).forEach(chapter=>{const option=element('option',null,chapterLabel(subject,chapter));option.value=chapter.id;select.append(option);});
  if ([...select.options].some(o=>o.value===current)) select.value=current;
}
function empty(title,text) { const box=element('div','library-empty');box.append(element('h2',null,title),element('p',null,text));return box; }
function orderButtons(type,id,index,length,disabled=false) {
  const box=element('div','order-buttons');
  for (const [symbol,delta,description] of [['↑',-1,'Mover para cima'],['↓',1,'Mover para baixo']]) {
    const control=button(symbol,'icon-button',()=>libraryAction(()=>window.study.reorderLibraryItem(type,id,delta).then(unwrap),'Ordem atualizada.'),description);
    control.disabled=disabled || (delta<0 ? index===0 : index===length-1);box.append(control);
  }
  return box;
}
async function createSubject() {
  const name=await ask({title:'Nova matéria',name:''});if (!name) return;
  await libraryAction(async()=>{selectedSubject=unwrap(await window.study.createSubject(name));selectedChapter=null;},'Matéria criada.');
}
async function createChapter() {
  if (!subjectNow()) return;
  const name=await ask({title:'Novo tema ou capítulo',name:''});if (!name)return;
  await libraryAction(async()=>{selectedChapter=unwrap(await window.study.createChapter(selectedSubject,name));},'Capítulo criado.');
}
function newInChapter() {
  if (!subjectNow() || !chapterNow()) return;
  L('#course').value=subjectNow().name;syncChapterOptions(selectedChapter);preview();setView('new');L('#subject').focus();
}
async function addDocuments() {
  if (!chapterNow()) return;
  let result;
  await libraryAction(async()=>{result=unwrap(await window.study.linkLibraryDocuments(selectedChapter));},()=>result ? `${result.added} arquivo(s) adicionado(s). ${result.skipped} já estava(m) na biblioteca.` : 'Nenhum arquivo adicionado.');
}
async function renameItem(type,id,name) {
  const changed=await ask({title:type==='chapter'?'Renomear capítulo':'Renomear anotação',name});
  if (changed && changed!==name) await libraryAction(()=>window.study.renameLibraryItem(type,id,changed).then(unwrap),'Nome atualizado. O arquivo original mantém seu nome.');
}
async function moveDocument(doc) {
  const destinations=catalog.subjects.flatMap(s=>s.chapters.map(c=>({id:c.id,name:`${s.name} / ${chapterLabel(s,c)}`})));
  if (!destinations.length)return;
  const target=await ask({title:'Organizar anotação em outro capítulo',destinations,selectedDestination:selectedChapter});
  if (target) await libraryAction(()=>window.study.moveLibraryDocument(doc.id,target).then(unwrap),'Anotação organizada. O arquivo não foi movido no disco.');
}
async function relinkDocument(id) {
  let updated=false;
  await libraryAction(async()=>{updated=unwrap(await window.study.relinkLibraryDocument(id));},()=>updated?'Referência atualizada.':'Nenhum arquivo selecionado. A referência anterior foi preservada.');
}
function renderLibrary() {
  const content=L('#library-content'), query=searchKey(L('#library-search').value.trim());content.replaceChildren();
  const subject=subjectNow();
  L('#library-search').placeholder=subject?'Buscar neste capítulo…':'Buscar matéria, capítulo ou anotação…';
  if (!subject) {
    const subjects=[...catalog.subjects].sort((a,b)=>compareNames.compare(a.name,b.name)).filter(s=>searchKey([s.name,...s.chapters.flatMap(c=>[c.name,...c.documents.map(d=>d.title)])].join(' ')).includes(query));
    if (!subjects.length) {content.append(empty(catalog.subjects.length?'Nenhum resultado':'Nenhuma matéria',catalog.subjects.length?'Tente outro nome.':'Crie uma matéria para adicionar anotações.'));return;}
    const grid=element('div','subjects-grid');
    for (const s of subjects) {
      const card=button('','card subject-card',()=>{selectedSubject=s.id;selectedChapter=s.chapters[0]?.id||null;L('#library-search').value='';renderLibrary();},`Abrir matéria ${s.name}`);
      card.append(element('span','card-arrow','↗'),element('span','subject-symbol','▤'),element('h2',null,s.name),element('p',null,`${counted(s.chapters.filter(c=>!c.default).length,'capítulo','capítulos')} · ${counted(s.chapters.reduce((n,c)=>n+c.documents.length,0),'anotação','anotações')}`));grid.append(card);
    }
    content.append(grid);return;
  }
  const breadcrumb=element('div','library-breadcrumb');
  breadcrumb.append(button('Matérias','text-button',()=>{selectedSubject=null;selectedChapter=null;L('#library-search').value='';renderLibrary();}),element('span',null,'/'),element('span',null,subject.name));content.append(breadcrumb);
  const layout=element('div','subject-detail'), chapters=element('aside','chapter-panel');chapters.setAttribute('aria-label','Capítulos da matéria');chapters.append(element('h2',null,'Temas e capítulos'));
  subject.chapters.forEach((chapter,i)=>{
    const row=element('div','chapter-row');
    const select=button('','chapter-select'+(selectedChapter===chapter.id?' selected':''),()=>{selectedChapter=chapter.id;renderLibrary();},`Abrir capítulo ${chapter.name}`);
    select.append(element('span','chapter-name',chapterLabel(subject,chapter)),element('span','chapter-count',counted(chapter.documents.length,'anotação','anotações')));
    row.append(select,orderButtons('chapter',chapter.id,i,subject.chapters.length));chapters.append(row);
  });
  chapters.append(button('＋ Novo capítulo','button secondary chapter-add',createChapter));layout.append(chapters);
  const documents=element('section','chapter-documents'), chapter=chapterNow();layout.append(documents);content.append(layout);
  if (!chapter) {documents.append(empty('Nenhum capítulo','Selecione “Novo capítulo” para adicionar um capítulo.'));return;}
  const header=element('div','chapter-header'), title=element('div');title.append(element('h2',null,chapter.name),element('p',null,`${counted(chapter.documents.length,'anotação','anotações')} · ordem ajustável`));
  header.append(title,button('Renomear capítulo','button ghost',()=>renameItem('chapter',chapter.id,chapter.name)));documents.append(header);
  const actions=element('div','chapter-actions');actions.append(button('＋ Nova anotação','button primary',newInChapter),button('Adicionar arquivos existentes','button secondary',addDocuments));documents.append(actions);
  const sortLabel=element('label','document-sort','Ordenar por');const sort=element('select');sort.setAttribute('aria-label','Ordenação das anotações');
  for (const [value,text] of [['manual','Ordem definida'],['name','Nome (1, 2, 10)'],['added','Data de inclusão']]){const option=element('option',null,text);option.value=value;sort.append(option);}sort.value=documentSort;sort.addEventListener('change',()=>{documentSort=sort.value;renderLibrary();});sortLabel.append(sort);documents.append(sortLabel);
  const docs=[...chapter.documents];if(documentSort==='name')docs.sort((a,b)=>compareNames.compare(a.title,b.title));if(documentSort==='added')docs.sort((a,b)=>a.addedAt.localeCompare(b.addedAt));
  const filtered=docs.filter(d=>searchKey(d.title).includes(query)||searchKey(chapter.name).includes(query)||searchKey(subject.name).includes(query));
  if (!filtered.length){documents.append(empty(query?'Nenhuma anotação encontrada':'Nenhuma anotação',query?'Tente outro nome.':'Adicione arquivos .xopp ou crie uma anotação neste capítulo.'));return;}
  const list=element('div','document-list');
  for(const doc of filtered){
    const index=docs.indexOf(doc),row=element('article','document-row'),info=element('div','document-info');
    info.append(button(doc.title,'document-title',()=>libraryAction(()=>window.study.openLibraryDocument(doc.id).then(unwrap),'Anotação aberta no Xournal++.'),`Abrir anotação ${doc.title}`));
    const detail=element('small',doc.available?'':'missing-document',doc.available?`Adicionado em ${new Date(doc.addedAt).toLocaleDateString('pt-BR')}`:'Arquivo não encontrado — localize-o para reabrir');info.append(detail);
    const controls=element('div','document-actions');
    if(!doc.available)controls.append(button('Localizar arquivo','button secondary',()=>relinkDocument(doc.id)));
    controls.append(button('Organizar','button secondary',()=>moveDocument(doc)),button('Renomear','button ghost',()=>renameItem('document',doc.id,doc.title)),button('Pasta','button ghost',()=>libraryAction(()=>window.study.showLibraryDocument(doc.id).then(unwrap),'Arquivo localizado na pasta.')),orderButtons('document',doc.id,index,docs.length,documentSort!=='manual'||!!query));
    row.append(element('span','document-number',String(index+1).padStart(2,'0')),info,controls);list.append(row);
  }
  documents.append(list);
}
L('#new-subject').addEventListener('click',createSubject);
L('#library-refresh').addEventListener('click',()=>libraryAction(async()=>{},'Biblioteca atualizada.'));
L('#library-search').addEventListener('input',renderLibrary);
L('#course').addEventListener('input',()=>syncChapterOptions());
window.libraryUi={load:loadLibrary,syncChapterOptions};

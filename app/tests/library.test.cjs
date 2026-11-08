const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { Library } = require('../library.cjs');

async function fixture(t) {
  const base=path.join(require('node:os').tmpdir(), 'archey-library-tests');
  await fs.mkdir(base,{recursive:true});
  const root=await fs.mkdtemp(path.join(base,'case-'));
  t.after(async()=>{
    const target=await fs.realpath(root);
    const relative=path.relative(base,target);
    assert.ok(relative && !relative.startsWith('..') && !path.isAbsolute(relative));
    await fs.rm(target,{recursive:true});
  });
  const library=new Library(root);
  const source=path.join(root,'Arquivo de estudo.xopp');
  await fs.writeFile(source,'test document bytes');
  return {root,source,library};
}
test('discovers existing lessons in natural order and ignores autosaves',async t=>{
  const {root,library}=await fixture(t);
  const directory=path.join(root,'documents/Aulas/E02/aulas');await fs.mkdir(directory,{recursive:true});
  for(const name of ['Aula 10.xopp','Aula 2.xopp','.Aula 1.autosave.xopp'])await fs.writeFile(path.join(directory,name),'sample');
  const catalog=await library.snapshot();
  assert.equal(catalog.subjects[0].name,'E02');
  assert.deepEqual(catalog.subjects[0].chapters[0].documents.map(d=>d.title),['Aula 2','Aula 10']);
  const again=await new Library(root).snapshot();
  assert.deepEqual(again,catalog);
});
test('chapters and document order persist; moving never moves or changes the files',async t=>{
  const {root,source,library}=await fixture(t);
  const subject=await library.createSubject('Circuitos elétricos');
  const first=await library.createChapter(subject,'Resistores');
  const second=await library.createChapter(subject,'Capacitores');
  const other=path.join(root,'Outra anotação.xopp');await fs.copyFile(source,other);
  assert.deepEqual(await library.linkDocuments(first,[source,other,source]),{added:2,skipped:1});
  let catalog=await library.snapshot();
  const id=catalog.subjects[0].chapters[0].documents[0].id;
  await library.reorder('document',id,1);
  catalog=await new Library(root).snapshot();
  assert.equal(catalog.subjects[0].chapters[0].documents[1].id,id);
  await library.move(id,second);await library.reorder('chapter',second,-1);
  catalog=await new Library(root).snapshot();
  assert.equal(catalog.subjects[0].chapters[0].id,second);
  assert.equal(catalog.subjects[0].chapters[0].documents[0].id,id);
  assert.equal(await fs.readFile(source,'utf8'),'test document bytes');
  assert.equal(await library.documentPath(id),await fs.realpath(source));
  assert.ok((await fs.readdir(path.join(root,'backups/library'))).length>0);
});
test('new lessons are registered in the selected chapter and cannot target another subject',async t=>{
  const {library,source}=await fixture(t);
  const subject=await library.createSubject('E02');const chapter=await library.createChapter(subject,'Leis de Kirchhoff');
  await library.checkTarget('e02',chapter);
  await assert.rejects(library.checkTarget('F01',chapter));
  await library.registerLesson(source,'Lei das malhas','E02',chapter);
  const catalog=await library.snapshot();
  assert.equal(catalog.subjects[0].chapters[0].documents[0].title,'Lei das malhas');
  assert.equal(catalog.subjects.length,1);
});
test('missing documents remain visible and can be reassociated without losing their place',async t=>{
  const {library,source,root}=await fixture(t);
  const subject=await library.createSubject('E02');const chapter=await library.createChapter(subject,'Capítulo 1');
  await library.linkDocuments(chapter,[source]);
  const id=(await library.snapshot()).subjects[0].chapters[0].documents[0].id;
  const relocated=path.join(root,'Reencontrado.xopp');await fs.rename(source,relocated);
  assert.equal((await library.snapshot()).subjects[0].chapters[0].documents[0].available,false);
  await assert.rejects(library.documentPath(id),/Arquivo não encontrado/);
  await library.relink(id,relocated);
  assert.equal((await library.snapshot()).subjects[0].chapters[0].documents[0].available,true);
});
test('invalid operations preserve the catalog; concurrent writes do not lose subjects',async t=>{
  const {library}=await fixture(t);
  await Promise.all(['E02','F01','M03'].map(name=>library.createSubject(name)));
  assert.equal((await library.snapshot()).subjects.length,3);
  const before=await fs.readFile(library.filename,'utf8');
  await assert.rejects(library.createSubject(' e02 '));
  await assert.rejects(library.createChapter('not-found','Capítulo'));
  await assert.rejects(library.reorder('document','not-found',3));
  assert.equal(await fs.readFile(library.filename,'utf8'),before);
});
test('corrupt catalog is never silently replaced',async t=>{
  const {library}=await fixture(t);
  await library.createSubject('E02');
  await fs.writeFile(library.filename,'{broken');
  await assert.rejects(library.snapshot(),/catálogo foi preservado/);
  assert.equal(await fs.readFile(library.filename,'utf8'),'{broken');
});
test('rediscovery does not undo a manual chapter classification',async t=>{
  const {root,library}=await fixture(t);
  const folder=path.join(root,'documents/Aulas/E02/Aula');await fs.mkdir(folder,{recursive:true});
  await fs.writeFile(path.join(folder,'01.xopp'),'sample');
  const initial=(await library.snapshot()).subjects[0];
  const target=await library.createChapter(initial.id,'Impedância');
  const document=initial.chapters[0].documents[0];await library.move(document.id,target);
  const after=(await new Library(root).snapshot()).subjects[0];
  assert.equal(after.chapters.find(c=>c.id===target).documents[0].id,document.id);
  assert.equal(after.chapters.reduce((n,c)=>n+c.documents.length,0),1);
});

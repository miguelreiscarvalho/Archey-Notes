const fs = require('node:fs/promises');
const path = require('node:path');
const { randomUUID } = require('node:crypto');

const natural = new Intl.Collator('pt-BR', { numeric: true, sensitivity: 'base' });
const key = value => value.trim().normalize('NFC').toLocaleLowerCase('pt-BR');
const fileKey = filename => path.resolve(filename).toLocaleLowerCase('pt-BR');
function label(value) {
  if (typeof value !== 'string' || !value.trim() || value.trim().length > 80 || /[\u0000-\u001f]/.test(value)) throw new Error('Use um nome de 1 a 80 caracteres.');
  return value.trim();
}
function findChapter(catalog, id) {
  for (const subject of catalog.subjects) {
    const chapter = subject.chapters.find(item => item.id === id);
    if (chapter) return { subject, chapter };
  }
  throw new Error('Capítulo não encontrado. Atualize a biblioteca.');
}
function findDocument(catalog, id) {
  for (const subject of catalog.subjects) for (const chapter of subject.chapters) {
    const document = chapter.documents.find(item => item.id === id);
    if (document) return { subject, chapter, document };
  }
  throw new Error('Anotação não encontrada. Atualize a biblioteca.');
}
function checkCatalog(catalog) {
  if (catalog.version !== 1 || !Array.isArray(catalog.subjects)) throw new Error('Formato de biblioteca inválido.');
  const ids = new Set();
  const checkId = item => {
    if (typeof item.id !== 'string' || !item.id || ids.has(item.id)) throw new Error('Identificador inválido na biblioteca.');
    ids.add(item.id);
  };
  for (const subject of catalog.subjects) {
    checkId(subject); label(subject.name);
    if (!Array.isArray(subject.chapters)) throw new Error('Capítulos inválidos.');
    for (const chapter of subject.chapters) {
      checkId(chapter); label(chapter.name);
      if (!Array.isArray(chapter.documents)) throw new Error('Anotações inválidas.');
      for (const doc of chapter.documents) {
        checkId(doc); label(doc.title);
        if (typeof doc.path !== 'string' || !path.isAbsolute(doc.path) || path.extname(doc.path).toLowerCase() !== '.xopp') throw new Error('Caminho inválido na biblioteca.');
        if (typeof doc.addedAt !== 'string' || !Number.isFinite(Date.parse(doc.addedAt))) throw new Error('Data inválida na biblioteca.');
      }
    }
  }
  return catalog;
}

class Library {
  constructor(root) {
    this.root = root;
    this.filename = path.join(root, 'profile/study-library/catalog.json');
    this.backups = path.join(root, 'backups/library');
    this.queue = Promise.resolve();
  }
  serial(callback) {
    const job = this.queue.then(callback);
    this.queue = job.catch(() => {});
    return job;
  }
  async read() {
    try { return checkCatalog(JSON.parse(await fs.readFile(this.filename, 'utf8'))); }
    catch (error) {
      if (error.code === 'ENOENT') return { version: 1, subjects: [] };
      throw new Error('Não foi possível ler a biblioteca. O catálogo foi preservado; consulte backups/library para recuperar uma versão.');
    }
  }
  async write(catalog) {
    checkCatalog(catalog);
    await fs.mkdir(path.dirname(this.filename), { recursive: true });
    await fs.mkdir(this.backups, { recursive: true });
    try { await fs.copyFile(this.filename, path.join(this.backups, `${Date.now()}-${randomUUID()}.json`)); }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
    const temporary = this.filename + '.' + randomUUID() + '.tmp';
    try {
      await fs.writeFile(temporary, JSON.stringify(catalog, null, 2), { encoding: 'utf8', flag: 'wx' });
      await fs.rename(temporary, this.filename);
    } finally { await fs.unlink(temporary).catch(() => {}); }
  }
  mutate(callback) {
    return this.serial(async () => {
      const catalog = await this.read();
      const result = await callback(catalog);
      await this.write(catalog);
      return result;
    });
  }
  subject(catalog, name) {
    name = label(name);
    let subject = catalog.subjects.find(item => key(item.name) === key(name));
    if (!subject) { subject = { id: randomUUID(), name, chapters: [] }; catalog.subjects.push(subject); }
    return subject;
  }
  unfiled(subject) {
    let chapter = subject.chapters.find(item => item.default === true);
    if (!chapter) { chapter = { id: randomUUID(), name: 'Sem capítulo', default: true, documents: [] }; subject.chapters.push(chapter); }
    return chapter;
  }
  async validFile(filename) {
    if (typeof filename !== 'string' || path.extname(filename).toLowerCase() !== '.xopp') throw new Error('Escolha um arquivo .xopp.');
    const resolved = await fs.realpath(filename);
    if (!(await fs.stat(resolved)).isFile()) throw new Error('Anotação não encontrada.');
    return resolved;
  }
  async snapshot() {
    return this.serial(async () => {
      const catalog = await this.read();
      const known = new Set(catalog.subjects.flatMap(s => s.chapters.flatMap(c => c.documents.map(d => fileKey(d.path)))));
      let changed = false;
      const base = path.join(this.root, 'documents/Aulas');
      const directories = await fs.readdir(base, { withFileTypes: true }).catch(error => { if (error.code === 'ENOENT') return []; throw error; });
      const walk = async folder => {
        const files = [];
        for (const item of await fs.readdir(folder, { withFileTypes: true })) {
          if (item.isSymbolicLink() || item.name.startsWith('.')) continue;
          const full = path.join(folder, item.name);
          if (item.isDirectory() && item.name !== 'PDF') files.push(...await walk(full));
          else if (item.isFile() && item.name.toLowerCase().endsWith('.xopp') && !item.name.toLowerCase().includes('.autosave.')) files.push(full);
        }
        return files;
      };
      for (const dir of directories.sort((a,b) => natural.compare(a.name,b.name))) {
        if (!dir.isDirectory() || dir.isSymbolicLink() || dir.name.startsWith('.')) continue;
        for (const filename of (await walk(path.join(base,dir.name))).sort(natural.compare)) {
          const resolved = await this.validFile(filename);
          if (known.has(fileKey(resolved))) continue;
          this.unfiled(this.subject(catalog,dir.name)).documents.push({ id: randomUUID(), title: path.basename(filename, path.extname(filename)).replace(/_/g,' ').slice(0,80), path: resolved, addedAt: new Date().toISOString() });
          known.add(fileKey(resolved)); changed = true;
        }
      }
      if (changed) await this.write(catalog);
      const result = structuredClone(catalog);
      for (const subject of result.subjects) for (const chapter of subject.chapters) {
        await Promise.all(chapter.documents.map(async doc => { doc.available = await fs.stat(doc.path).then(s => s.isFile(), () => false); }));
      }
      return result;
    });
  }
  createSubject(name) {
    return this.mutate(catalog => {
      name = label(name);
      if (catalog.subjects.some(s => key(s.name) === key(name))) throw new Error('Essa matéria já existe.');
      return this.subject(catalog,name).id;
    });
  }
  createChapter(subjectId, name) {
    return this.mutate(catalog => {
      const subject = catalog.subjects.find(s => s.id === subjectId);
      if (!subject) throw new Error('Matéria não encontrada.');
      name = label(name);
      if (subject.chapters.some(c => key(c.name) === key(name))) throw new Error('Esse capítulo já existe nesta matéria.');
      const chapter = { id: randomUUID(), name, documents: [] }; subject.chapters.push(chapter); return chapter.id;
    });
  }
  async checkTarget(course, chapterId) {
    if (!chapterId) return;
    if (typeof chapterId !== 'string') throw new Error('Capítulo inválido.');
    return this.serial(async () => {
      const { subject } = findChapter(await this.read(),chapterId);
      if (key(subject.name) !== key(course)) throw new Error('O capítulo pertence a outra matéria.');
    });
  }
  async registerLesson(filename, title, course, chapterId) {
    const resolved = await this.validFile(filename);
    return this.mutate(catalog => {
      const subject = this.subject(catalog,course);
      const target = chapterId ? findChapter(catalog,chapterId) : { subject, chapter: this.unfiled(subject) };
      if (target.subject.id !== subject.id) throw new Error('Capítulo de outra matéria.');
      const existing = catalog.subjects.flatMap(s=>s.chapters.flatMap(c=>c.documents)).find(d=>fileKey(d.path)===fileKey(resolved));
      if (existing) return existing.id;
      const document = { id: randomUUID(), title: label(title.slice(0,80)), path: resolved, addedAt: new Date().toISOString() };
      target.chapter.documents.push(document); return document.id;
    });
  }
  async linkDocuments(chapterId, filenames) {
    if (!Array.isArray(filenames) || !filenames.length) throw new Error('Selecione uma anotação.');
    const resolved = await Promise.all(filenames.map(f=>this.validFile(f)));
    return this.mutate(catalog => {
      const { chapter } = findChapter(catalog,chapterId);
      const known = new Set(catalog.subjects.flatMap(s=>s.chapters.flatMap(c=>c.documents.map(d=>fileKey(d.path)))));
      let added = 0;
      for (const filename of resolved) {
        if (known.has(fileKey(filename))) continue;
        chapter.documents.push({ id: randomUUID(), title: path.basename(filename,path.extname(filename)).slice(0,80), path: filename, addedAt: new Date().toISOString() });
        known.add(fileKey(filename)); added++;
      }
      return { added, skipped: filenames.length-added };
    });
  }
  move(documentId, chapterId) {
    return this.mutate(catalog => {
      const from = findDocument(catalog,documentId), to = findChapter(catalog,chapterId);
      if (from.chapter.id !== to.chapter.id) {
        from.chapter.documents.splice(from.chapter.documents.indexOf(from.document),1);
        to.chapter.documents.push(from.document);
      }
    });
  }
  reorder(type, id, delta) {
    if (!['chapter','document'].includes(type) || ![-1,1].includes(delta)) return Promise.reject(new Error('Ordem inválida.'));
    return this.mutate(catalog => {
      const found = type==='chapter' ? findChapter(catalog,id) : findDocument(catalog,id);
      const list = type==='chapter' ? found.subject.chapters : found.chapter.documents;
      const index = list.findIndex(item=>item.id===id), destination = index+delta;
      if (destination>=0 && destination<list.length) [list[index],list[destination]]=[list[destination],list[index]];
    });
  }
  rename(type,id,name) {
    return this.mutate(catalog => {
      name = label(name);
      if (type==='document') findDocument(catalog,id).document.title=name;
      else if (type==='chapter') {
        const { subject, chapter }=findChapter(catalog,id);
        if (subject.chapters.some(c=>c.id!==id && key(c.name)===key(name))) throw new Error('Já existe um capítulo com esse nome.');
        chapter.name=name;
        if (chapter.default && key(name)!==key('Sem capítulo')) delete chapter.default;
      } else throw new Error('Tipo inválido.');
    });
  }
  async documentPath(id) {
    return this.serial(async () => {
      const { document }=findDocument(await this.read(),id);
      try { return await this.validFile(document.path); }
      catch { throw new Error('Arquivo não encontrado. Use Localizar arquivo para reassociar a anotação.'); }
    });
  }
  async relink(id,filename) {
    const resolved=await this.validFile(filename);
    return this.mutate(catalog => {
      const { document }=findDocument(catalog,id);
      if (catalog.subjects.some(s=>s.chapters.some(c=>c.documents.some(d=>d.id!==id && fileKey(d.path)===fileKey(resolved))))) throw new Error('Esse arquivo já está na biblioteca.');
      document.path=resolved;
    });
  }
}
module.exports={ Library, natural };

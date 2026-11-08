const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { editorExecutable } = require('../editor-runtime.cjs');

test('opens stock or adapted editor according to the isolated laboratory manifest', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'xournal-editor-'));
  try {
    const stock = path.join(root,'app/1.3.8/xournalpp-1.3.8-windows-portable-AMD64/bin/xournalpp.exe');
    await fs.mkdir(path.dirname(stock), { recursive: true }); await fs.writeFile(stock,'fixture');
    assert.equal(await editorExecutable(root),stock);
    const adapted = path.join(root,'app/1.3.8-tracos/bin/xournalpp.exe');
    await fs.mkdir(path.dirname(adapted),{recursive:true});await fs.writeFile(adapted,'fixture');
    await fs.mkdir(path.join(root,'profile'));
    const manifest = path.join(root,'profile/editor-runtime.json');
    await fs.writeFile(manifest,JSON.stringify({version:1,executable:'app/1.3.8-tracos/bin/xournalpp.exe'}));
    assert.equal(await editorExecutable(root),adapted);
    await fs.writeFile(manifest,'\uFEFF'+JSON.stringify({version:1,executable:'app/1.3.8-tracos/bin/xournalpp.exe'}));
    assert.equal(await editorExecutable(root),adapted);
    for(const executable of ['../outside/xournalpp.exe','app/other.exe']) {
      await fs.writeFile(manifest,JSON.stringify({version:1,executable}));
      await assert.rejects(editorExecutable(root),/laboratório/);
    }
    await fs.writeFile(manifest,'corrupt');
    await assert.rejects(editorExecutable(root));
  } finally {
    assert.equal(path.dirname(path.resolve(root)),path.resolve(os.tmpdir()));
    assert.ok(path.basename(root).startsWith('xournal-editor-'));
    await fs.rm(root,{recursive:true,force:true});
  }
});

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { createHash } = require('node:crypto');
const { runWorkflow } = require('../backend.cjs');

test('Node invokes the real Python backend with Unicode paths and preserves the source', {skip: !process.env.XOURNAL_LAB_ROOT}, async () => {
  const root = process.env.XOURNAL_LAB_ROOT;
  const folder = path.join(root, 'tests/electron-bridge');
  await fs.mkdir(folder, { recursive: true });
  const source = path.join(folder, 'Anotação — integração Electron.xopp');
  await fs.copyFile(path.join(root, 'documents/Modelos/Quadriculado.xopp'), source);
  const before = createHash('sha256').update(await fs.readFile(source)).digest('hex');
  const output = await runWorkflow(root, ['pack', source, path.join(folder, 'packages')]);
  assert.ok(output.endsWith('.zip'));
  assert.ok((await fs.stat(output)).size > 0);
  assert.equal(createHash('sha256').update(await fs.readFile(source)).digest('hex'), before);
});

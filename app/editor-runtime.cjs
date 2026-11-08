const fs = require('node:fs/promises');
const path = require('node:path');

async function editorExecutable(root) {
  let relative = 'app/1.3.8/xournalpp-1.3.8-windows-portable-AMD64/bin/xournalpp.exe';
  try {
    const config = JSON.parse((await fs.readFile(path.join(root, 'profile/editor-runtime.json'), 'utf8')).replace(/^\uFEFF/, ''));
    if (config.version !== 1 || typeof config.executable !== 'string') throw new Error('Configuração do editor inválida.');
    relative = config.executable;
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
  const executable = path.resolve(root, relative);
  const within = path.relative(root, executable);
  if (!within || within.startsWith('..') || path.isAbsolute(within) || path.basename(executable).toLowerCase() !== 'xournalpp.exe') {
    throw new Error('O executável deve pertencer ao laboratório.');
  }
  await fs.access(executable);
  return executable;
}
module.exports = { editorExecutable };

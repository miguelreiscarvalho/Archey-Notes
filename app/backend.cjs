const { execFile } = require('node:child_process');
const path = require('node:path');
const fs = require('node:fs/promises');
const { existsSync } = require('node:fs');

function validateLesson(input, models) {
  if (!input || typeof input !== 'object') throw new Error('Preencha os dados da aula.');
  const values = {};
  for (const [key, max] of [['course', 80], ['title', 300], ['date', 10], ['template', 80]]) {
    if (typeof input[key] !== 'string' || !input[key].trim() || input[key].length > max || /[\u0000-\u001f]/.test(input[key])) {
      throw new Error('Confira a disciplina, o assunto, a data e o modelo.');
    }
    values[key] = input[key].trim();
  }
  if (!/^\d{4}-\d{2}-\d{2}$/.test(values.date)) throw new Error('Informe uma data válida.');
  const parsed = new Date(values.date + 'T12:00:00Z');
  if (Number.isNaN(parsed.getTime()) || parsed.toISOString().slice(0, 10) !== values.date) throw new Error('Informe uma data válida.');
  if (!models.includes(values.template)) throw new Error('Escolha um dos modelos disponíveis.');
  return values;
}

function lessonArgs(values, pdf) {
  const args = ['new', `--course=${values.course}`, `--title=${values.title}`, `--date=${values.date}`, `--template=${values.template}`];
  if (pdf) args.push(`--pdf=${pdf}`);
  return args;
}

function runWorkflow(root, args) {
  return new Promise((resolve, reject) => {
    const portablePython = path.join(root, 'tools/python/python.exe');
    execFile(existsSync(portablePython) ? portablePython : path.join(root, 'tools/qa/Scripts/python.exe'), ['-X', 'utf8', path.join(root, 'launcher/StudyWorkflow.py'), ...args], {
      windowsHide: true, shell: false, encoding: 'utf8', timeout: 180000,
      maxBuffer: 1024 * 1024, env: { ...process.env, PYTHONUTF8: '1' }
    }, async (error, stdout, stderr) => {
      if (error) {
        // The backend's final exception line carries the useful error, without surfacing a Python traceback in the UI.
        const message = stderr.trim().split(/\r?\n/).filter(Boolean).at(-1) || 'Não foi possível concluir a operação.';
        reject(new Error(message.replace(/^[\w.]+Error:\s*/, '')));
      } else {
        const output = stdout.trim();
        const extension = { new: '.xopp', export: '.pdf', backup: '.zip', pack: '.zip' }[args[0]];
        try {
          if (!path.isAbsolute(output) || path.extname(output).toLowerCase() !== extension || !(await fs.stat(output)).isFile()) throw new Error('Resultado inválido');
          resolve(output);
        } catch { reject(new Error('A operação não retornou um arquivo válido.')); }
      }
    });
  });
}

module.exports = { validateLesson, lessonArgs, runWorkflow };

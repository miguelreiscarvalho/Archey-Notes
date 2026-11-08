const test = require('node:test');
const assert = require('node:assert/strict');
const { validateLesson, lessonArgs } = require('../backend.cjs');
const models = ['Quadriculado', 'Branco'];
const example = { course: 'E02', title: 'Tensão & corrente', date: '2026-11-06', template: 'Quadriculado' };
test('preserves Portuguese and trims fields', () => {
  const values = validateLesson({ ...example, course: ' E02 ' }, models);
  assert.equal(values.course, 'E02'); assert.equal(values.title, example.title);
});
test('rejects invalid dates, control characters and unknown templates', () => {
  for (const changed of [{ date: '2026-02-31' }, { title: 'a\nb' }, { template: '../Branco' }, { title: '' }]) {
    assert.throws(() => validateLesson({ ...example, ...changed }, models));
  }
});
test('accepts leap day only in a leap year', () => {
  assert.equal(validateLesson({ ...example, date: '2028-02-29' }, models).date, '2028-02-29');
  assert.throws(() => validateLesson({ ...example, date: '2027-02-29' }, models));
});
test('passes text and file paths as separate arguments rather than shell commands', () => {
  const title = 'Lei "de Ohm" & corrente; $(teste)';
  const pdf = 'D:\\Materiais de aula\\Tensão & corrente.pdf';
  const args = lessonArgs({ ...example, title }, pdf);
  assert.ok(args.includes('--title=' + title));
  assert.equal(args.at(-1), '--pdf=' + pdf);
  assert.equal(args[0], 'new');
  assert.ok(lessonArgs({ ...example, title: '--help' }).includes('--title=--help'));
});

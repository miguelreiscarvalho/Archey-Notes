"""Local study workflow. No cloud API or automatic uploads."""
import argparse
import datetime as dt
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unicodedata
import uuid
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(os.environ.get('XOURNAL_LAB_ROOT', Path(__file__).resolve().parent.parent))

def slug(value):
    value = unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode()
    value = re.sub(r'[^a-zA-Z0-9_-]+', '-', value).strip('-_')[:70]
    if not value or value.upper() in {'CON','PRN','AUX','NUL',*(f'COM{i}' for i in range(1,10)),*(f'LPT{i}' for i in range(1,10))}:
        raise ValueError('Informe um nome válido.')
    return value

def stamp():
    return dt.datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '-' + uuid.uuid4().hex[:6]

def read_doc(path):
    path = Path(path)
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            entries = {n:z.read(n) for n in z.namelist() if not n.endswith('/')}
        return ET.fromstring(entries['content.xml']), entries
    return ET.fromstring(gzip.decompress(path.read_bytes())), None

def bundle(path, destination):
    """Snapshot the saved document, copying external backgrounds into a portable ZIP."""
    path = Path(path).resolve(strict=True)
    original = path.read_bytes()
    doc, entries = read_doc(path)
    assets = {}
    if doc.find('.//audiofile') is not None:
        raise ValueError('Documento com áudio: use uma cópia completa da pasta; empacotamento de áudio não disponível.')
    for node in doc.iter():
        filename, domain = node.get('filename'), node.get('domain')
        if not filename or domain not in ('absolute','attach'):
            continue
        if domain == 'attach' and entries is not None:
            continue  # already embedded inside the original ZIP-format XOPP
        source = Path(str(path)+'.'+filename) if domain == 'attach' else Path(filename)
        if not source.is_absolute():
            source = path.parent / source
        payload = source.read_bytes()  # missing asset aborts instead of producing an incomplete package
        target = 'assets/' + hashlib.sha256(payload).hexdigest()[:16] + source.suffix.lower()
        assets[target] = payload
        node.set('domain', 'absolute')  # native Xournal++ also accepts relative paths under this label
        node.set('filename', target)
    xml = ET.tostring(doc, encoding='utf-8', xml_declaration=True)
    if entries is None:
        xopp = gzip.compress(xml)
    else:
        entries['content.xml'] = xml
        import io
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer,'w',zipfile.ZIP_DEFLATED) as z:
            for name, payload in entries.items():
                z.writestr(name,payload)
        xopp = buffer.getvalue()
    if path.read_bytes() != original:
        raise RuntimeError('O arquivo mudou durante a cópia. Salve e tente novamente.')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    output = destination / (slug(path.stem) + '_' + stamp() + '.zip')
    payloads = {path.name:xopp, **assets}
    manifest = {'source':path.name,'created':dt.datetime.now().astimezone().isoformat(),
                'files':{n:hashlib.sha256(b).hexdigest() for n,b in payloads.items()}}
    temporary = output.with_suffix('.partial')
    try:
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED) as z:
            for name,payload in payloads.items(): z.writestr(name,payload)
            z.writestr('manifest.json',json.dumps(manifest,ensure_ascii=False,indent=2))
            z.writestr('LEIA-ME.txt','Extraia todo o pacote em uma pasta antes de abrir o arquivo .xopp.\n')
        with zipfile.ZipFile(temporary) as z:
            if z.testzip(): raise RuntimeError('Falha na verificação do pacote.')
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    return output

def create_lesson(course, title, date, template='Quadriculado', pdf=None):
    date = dt.date.fromisoformat(date).isoformat()
    course_id, subject = slug(course).upper()[:12], slug(title).lower()[:40]
    parent = ROOT / 'documents/Aulas' / course_id
    parent.mkdir(parents=True,exist_ok=True)
    base = f'{date}_{course_id}_{subject}'
    folder = parent / base
    for i in range(1000):
        candidate = folder if i == 0 else parent / (base + f'-{i+1}')
        try:
            candidate.mkdir()
            folder = candidate
            break
        except FileExistsError:
            continue
    else: raise RuntimeError('Muitas aulas com o mesmo nome.')
    try:
        output = folder / (folder.name+'.xopp')
        if pdf:
            import pymupdf
            shutil.copy2(pdf,folder/'material.pdf')
            doc = ET.Element('xournal',creator='Xournal++ 1.3.8',fileversion='4')
            with pymupdf.open(folder/'material.pdf') as source:
                if source.needs_pass or not len(source): raise ValueError('PDF vazio ou protegido por senha.')
                for i,page in enumerate(source):
                    p=ET.SubElement(doc,'page',width=str(page.rect.width),height=str(page.rect.height))
                    bg=ET.SubElement(p,'background',type='pdf',pageno=str(i+1))
                    if i == 0: bg.attrib.update(domain='absolute',filename='material.pdf')
                    ET.SubElement(p,'layer')
        else:
            templates = {p.stem:p for p in (ROOT/'documents/Modelos').glob('*.xopp')}
            if template not in templates: raise ValueError('Modelo não encontrado.')
            doc,_=read_doc(templates[template])
            texts=doc.find('page/layer').findall('text')
            for node,text in zip(texts,[title, 'Disciplina: '+course_id, 'Assunto: '+title, 'Data: '+date]):
                node.text=text
        output.write_bytes(gzip.compress(ET.tostring(doc,encoding='utf-8',xml_declaration=True)))
        (folder/'PDF').mkdir()
        bundle(output,ROOT/'backups/aulas'/course_id)
        return output
    except Exception:
        # Preserve any created files for diagnosis; never recursively remove a user folder.
        (folder/'ERRO.txt').write_text('Criação incompleta. Verifique o material e tente novamente.',encoding='utf-8')
        raise

def export_saved(path):
    path=Path(path).resolve(strict=True)
    bundle(path,ROOT/'backups/aulas'/slug(path.parent.name))
    destination=path.parent/'PDF'
    destination.mkdir(exist_ok=True)
    output=destination/(slug(path.stem)+'_anotado_'+stamp()+'.pdf')
    env=os.environ.copy()
    for key,folder in [('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('TEMP','temp'),('TMP','temp')]:
        # CLI checks use a separate profile to avoid overwriting preferences in a running editor.
        directory=ROOT/'profile/workflow'/folder
        directory.mkdir(parents=True,exist_ok=True)
        env[key]=str(directory)
    runtime = json.loads((ROOT/'profile/editor-runtime.json').read_text(encoding='utf-8-sig'))
    exe=(ROOT/runtime['executable']).resolve()
    if not exe.is_relative_to(ROOT.resolve()) or exe.name.lower() != 'xournalpp.exe':
        raise ValueError('Executável fora da instalação.')
    p=subprocess.run([str(exe),'--create-pdf',str(output),str(path)],env=env,capture_output=True,timeout=120)
    if p.returncode or not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError('Falha ao exportar: '+p.stderr.decode(errors='replace'))
    return output

def launch(path):
    subprocess.Popen(['powershell.exe','-NoProfile','-WindowStyle','Hidden','-ExecutionPolicy','Bypass','-File',str(ROOT/'launcher/Start-XournalLab.ps1'),'-Document',str(path)],creationflags=subprocess.CREATE_NO_WINDOW)

def gui():
    # Compatibility entry point: all visual interfaces now use Electron.
    subprocess.Popen(['powershell.exe', '-NoProfile', '-WindowStyle', 'Hidden',
        '-ExecutionPolicy', 'Bypass', '-File', str(ROOT/'launcher/Start-StudyPanel.ps1')],
        creationflags=subprocess.CREATE_NO_WINDOW)

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    sub=parser.add_subparsers(dest='command')
    new=sub.add_parser('new')
    for flag in ('course','title','date'):new.add_argument('--'+flag,required=True)
    new.add_argument('--template',default='Quadriculado');new.add_argument('--pdf')
    for name in ('export','backup','pack'):
        p=sub.add_parser(name);p.add_argument('path')
        if name=='pack':p.add_argument('destination')
    args=parser.parse_args()
    if args.command=='new': print(create_lesson(args.course,args.title,args.date,args.template,args.pdf))
    elif args.command=='export':print(export_saved(args.path))
    elif args.command=='backup':print(bundle(args.path,ROOT/'backups/aulas/manuais'))
    elif args.command=='pack':print(bundle(args.path,args.destination))
    else:gui()

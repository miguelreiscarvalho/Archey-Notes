(() => {
  const home=document.querySelector('#workspace-home'), editor=document.querySelector('#workspace-editor'), surface=document.querySelector('#editor-surface');
  let view='home',ready=false,scheduled=false,reservedRight=null;
  async function updateLayout(){
    scheduled=false;
    const sidebar=document.querySelector('.sidebar').getBoundingClientRect();
    const x=Math.max(0,(reservedRight===null?sidebar.right:Math.max(sidebar.right,reservedRight))+16),y=8,width=Math.max(1,innerWidth-x-8),height=Math.max(1,innerHeight-y-8);
    Object.assign(surface.style,{left:x+'px',top:y+'px',width:width+'px',height:height+'px'});
    await window.study.workspaceLayout({x,y,width,height,viewportWidth:innerWidth,viewportHeight:innerHeight,visible:view==='editor'});
  }
  function schedule(){if(!scheduled){scheduled=true;requestAnimationFrame(updateLayout);}}
  function show(next){
    view=next;editor.disabled=!ready;document.querySelector('#editor-loading').hidden=ready;
    document.documentElement.dataset.workspace=view;surface.hidden=view!=='editor';
    editor.classList.toggle('active',view==='editor');
    if(view==='editor')editor.setAttribute('aria-current','page');else editor.removeAttribute('aria-current');
    document.querySelectorAll('[data-view]').forEach(button=>{
      const active=view==='home'&&button.dataset.view===currentView;
      button.classList.toggle('active',active);
      if(active)button.setAttribute('aria-current','page');else button.removeAttribute('aria-current');
    });
    schedule();
  }
  async function run(fn){try{const r=await fn();if(!r.ok)throw new Error(r.error);if(r.data){ready=true;editor.disabled=false;show('editor');}}catch(error){document.querySelector('#status').textContent=error.message;document.querySelector('#status').classList.add('error');show('home');}}
  home.addEventListener('click',()=>show('home'));editor.addEventListener('click',()=>{if(ready)show('editor');});
  document.querySelector('#workspace-open').addEventListener('click',()=>run(()=>window.study.workspaceOpen()));
  document.querySelector('#workspace-new').addEventListener('click',()=>run(()=>window.study.workspaceNew()));
  document.querySelectorAll('[data-view]').forEach(button=>button.addEventListener('click',()=>show('home')));
  window.study.onWorkspaceEvent(event=>{
    if(event.type==='home'){setView('library');show('home');}
    else if(event.type==='show'){ready=true;editor.disabled=false;show('editor');}
    else if(event.type==='closed'){ready=false;editor.disabled=true;show('home');}
    else if(event.type==='error'){document.querySelector('#status').textContent=event.message;show('home');}
  });
  window.addEventListener('navigation:motion-start',()=>{const sidebar=document.querySelector('.sidebar').getBoundingClientRect();reservedRight=sidebar.left+264;schedule();});
  window.addEventListener('navigation:motion-end',()=>{reservedRight=null;schedule();});
  new ResizeObserver(schedule).observe(document.querySelector('.sidebar'));
  new MutationObserver(schedule).observe(document.documentElement,{attributes:true,attributeFilter:['data-nav']});
  addEventListener('resize',schedule);addEventListener('workspace:sync',()=>{editor.disabled=!ready;});show('home');
  window.workspaceUi={show,updateLayout};
})();

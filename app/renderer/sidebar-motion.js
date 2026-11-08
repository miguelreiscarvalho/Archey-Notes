(() => {
  const root=document.documentElement,sidebar=document.querySelector('.sidebar'),nav=document.querySelector('#panel-navigation');
  const buttons=[...nav.querySelectorAll('.nav-item')];
  const indicator=document.createElement('div');indicator.className='nav-motion-indicator';indicator.setAttribute('aria-hidden','true');nav.prepend(indicator);
  let hovered=null,focused=null,frame=0,motionTimer=0;
  function update(){
    frame=0;const target=hovered||focused||buttons.find(b=>b.classList.contains('active'));
    if(!target||target.disabled){indicator.dataset.visible='false';return;}
    const rect=target.getBoundingClientRect(),container=nav.getBoundingClientRect();
    indicator.style.transform=`translateY(${rect.top-container.top+nav.scrollTop}px)`;indicator.style.height=rect.height+'px';indicator.dataset.visible='true';
  }
  function schedule(){if(!frame)frame=requestAnimationFrame(update);}
  buttons.forEach(button=>{
    button.addEventListener('pointerenter',()=>{if(!button.disabled){hovered=button;schedule();}});
    button.addEventListener('focus',()=>{focused=button;schedule();});
    button.addEventListener('blur',()=>{focused=null;schedule();});
  });
  nav.addEventListener('pointerleave',()=>{hovered=null;schedule();});nav.addEventListener('scroll',schedule,{passive:true});
  new MutationObserver(schedule).observe(nav,{subtree:true,attributes:true,attributeFilter:['class','disabled','aria-current']});
  new ResizeObserver(schedule).observe(sidebar);
  function end(){clearTimeout(motionTimer);motionTimer=0;window.dispatchEvent(new Event('navigation:motion-end'));schedule();}
  sidebar.addEventListener('transitionend',event=>{if(event.target===sidebar&&event.propertyName==='width')end();});
  window.addEventListener('navigation:motion-start',event=>{clearTimeout(motionTimer);const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;motionTimer=setTimeout(end,reduced?0:(event.detail.collapsed?410:560));});
  requestAnimationFrame(()=>{root.classList.add('nav-motion-ready');schedule();});
  window.sidebarMotion={refresh:schedule};
})();

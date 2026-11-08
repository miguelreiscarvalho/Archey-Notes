const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const path=require('node:path');
module.exports=async({window,app})=>{
  const folder=process.env.XOURNAL_WORKSPACE_QA;if(!folder)throw new Error('Use um perfil de ensaio.');
  try{
    const result=await window.webContents.executeJavaScript(`(async()=>{
      await window.panelReady;
      const sleep=ms=>new Promise(r=>setTimeout(r,ms));
      const sidebar=document.querySelector('.sidebar'),toggle=document.querySelector('#toggle-nav'),label=document.querySelector('.brand .nav-copy');
      const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;
      const centeredBrand=()=>{
        const box=document.querySelector('.brand-icon').getBoundingClientRect(),img=document.querySelector('.brand-icon img');
        const rect=img.getBoundingClientRect();
        return img.complete&&img.naturalWidth>0&&Math.abs((box.left+box.right)-(rect.left+rect.right))<1&&Math.abs((box.top+box.bottom)-(rect.top+rect.bottom))<1;
      };
      applyNavigation(false);await sleep(750);
      const expanded=sidebar.getBoundingClientRect().width;
      const expandedBrandCentered=centeredBrand();
      toggle.click();await sleep(160);const closing=sidebar.getBoundingClientRect().width;await sleep(500);
      const collapsed=sidebar.getBoundingClientRect().width,collapsedText=getComputedStyle(label).opacity;
      const collapsedBrandCentered=centeredBrand();
      const rail=sidebar.getBoundingClientRect(),brand=document.querySelector('.brand-icon').getBoundingClientRect();
      const brandCenteredInRail=Math.abs((rail.left+rail.right)-(brand.left+brand.right))<1;
      toggle.click();await sleep(180);const opening=sidebar.getBoundingClientRect().width;await sleep(650);
      const reopened=sidebar.getBoundingClientRect().width,expandedText=getComputedStyle(label).opacity;
      const target=document.querySelector('[data-view=export]'),indicator=document.querySelector('.nav-motion-indicator');
      target.dispatchEvent(new PointerEvent('pointerenter'));await sleep(320);
      const hoverAligned=Math.abs(indicator.getBoundingClientRect().top-target.getBoundingClientRect().top)<1;
      document.querySelector('#panel-navigation').dispatchEvent(new PointerEvent('pointerleave'));await sleep(320);
      const active=document.querySelector('#workspace-home');
      const returnsToActive=Math.abs(indicator.getBoundingClientRect().top-active.getBoundingClientRect().top)<1;
      target.focus();await sleep(320);
      const keyboardAligned=Math.abs(indicator.getBoundingClientRect().top-target.getBoundingClientRect().top)<1;
      toggle.focus();await sleep(320);
      return {expanded,closing,collapsed,opening,reopened,collapsedText,expandedText,reduced,hoverAligned,returnsToActive,keyboardAligned,expandedBrandCentered,collapsedBrandCentered,brandCenteredInRail};
    })()`);
    assert.equal(result.expanded,264);assert.equal(result.collapsed,88);assert.equal(result.reopened,264);
    if(!result.reduced){assert.ok(result.closing>88&&result.closing<264);assert.ok(result.opening>88&&result.opening<264);}
    assert.equal(Number(result.collapsedText),0);assert.equal(Number(result.expandedText),1);
    assert.ok(result.hoverAligned&&result.returnsToActive&&result.keyboardAligned,JSON.stringify(result));
    assert.ok(result.expandedBrandCentered&&result.collapsedBrandCentered&&result.brandCenteredInRail,JSON.stringify(result));
    await fs.writeFile(path.join(folder,'sidebar-motion-result.json'),JSON.stringify(result,null,2));
    await fs.writeFile(path.join(folder,'sidebar-expanded.png'),(await window.webContents.capturePage()).toPNG());
    await window.webContents.executeJavaScript("applyTheme('dark');applyNavigation(true);new Promise(r=>setTimeout(r,700))");
    await fs.writeFile(path.join(folder,'sidebar-collapsed-dark.png'),(await window.webContents.capturePage()).toPNG());
    console.log(JSON.stringify(result));app.quit();
  }catch(error){console.error(error);app.exit(1);}
};

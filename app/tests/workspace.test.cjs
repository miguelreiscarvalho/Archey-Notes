const test=require('node:test');
const assert=require('node:assert/strict');
const {layout}=require('../native-workspace.cjs');
test('validates the native editor viewport and explicit visibility',()=>{
  const value={x:80,y:124,width:1000,height:680,viewportWidth:1200,viewportHeight:830,visible:true};
  assert.deepEqual(layout(value),value);
  assert.equal(layout({...value,visible:'yes'}).visible,false);
  for(const bad of [{...value,x:-1},{...value,width:NaN},{...value,height:Infinity},{...value,viewportWidth:0},{...value,y:30000}])assert.throws(()=>layout(bad));
});

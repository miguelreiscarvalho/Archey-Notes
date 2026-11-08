using System;
using System.Linq;
using System.Collections.Generic;
using System.Text;

namespace Tracos {
    public static class CharacterCoverage {
        // A validation target, not a claim that a statistical recognizer
        // reliably identifies every handwritten form of every character.
        public static readonly string PrintableAscii=new string(Enumerable.Range(32,95).Select(c=>(char)c).ToArray());
        public const string Portuguese="ÁÀÂÃÉÊÍÓÔÕÚÜÇáàâãéêíóôõúüç";
        public static string Target { get {return PrintableAscii+Portuguese;} }
    }
    public static class PunctuationCandidates {
        public static string Select(string top,IEnumerable<string> alternatives,IList<InkTrace> traces) {
            // Resolve the documented native ':' vs degree ambiguity only when
            // TWO original dot-sized strokes are vertically stacked. Never
            // invent a candidate the engine did not return or rewrite letters.
            if(traces.Count!=2 || !traces.All(Spatial.IsDot))return top;
            var a=traces[0].Box;var b=traces[1].Box;
            double gap=Math.Max(a.Y-b.Bottom,b.Y-a.Bottom);
            double offset=Math.Abs((a.X+a.Right-b.X-b.Right)/2);
            if(gap<3 || gap>24 || offset>6)return top;
            if(top!=":" && top!="°" && top!="º" && top!="o" && top!="O")return top;
            return alternatives.Contains(":")?":":top;
        }
    }
}

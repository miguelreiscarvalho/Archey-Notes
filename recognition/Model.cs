using System;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.Serialization;
using System.IO;
using System.Runtime.Serialization.Json;

namespace Tracos {
    [DataContract] public class InkPoint {
        [DataMember] public double X;
        [DataMember] public double Y;
        [DataMember] public long Time;
        [DataMember] public float Pressure;
        [DataMember] public float? TiltX;
        [DataMember] public float? TiltY;
    }
    [DataContract] public class InkTrace {
        [DataMember] public string Id = Guid.NewGuid().ToString();
        [DataMember] public List<InkPoint> Points = new List<InkPoint>();
        [DataMember] public string Device = "unknown";
        int cachedCount=-1;
        Bounds cachedBounds;
        public Bounds Box { get {
            if(cachedCount!=Points.Count){cachedBounds=Bounds.Of(Points);cachedCount=Points.Count;}
            return cachedBounds;
        } }
    }
    public struct Bounds {
        public double X,Y,Right,Bottom;
        public double Height { get { return Bottom-Y; } }
        public static Bounds Of(IEnumerable<InkPoint> points) {
            bool any=false;var box=new Bounds();
            foreach(var p in points){if(!any){box=new Bounds{X=p.X,Y=p.Y,Right=p.X,Bottom=p.Y};any=true;}
                else{box.X=Math.Min(box.X,p.X);box.Y=Math.Min(box.Y,p.Y);box.Right=Math.Max(box.Right,p.X);box.Bottom=Math.Max(box.Bottom,p.Y);}}
            return box;
        }
        public static Bounds Union(IEnumerable<Bounds> bounds) {
            bool any=false;var box=new Bounds();
            foreach(var b in bounds){if(!any){box=b;any=true;}else{box.X=Math.Min(box.X,b.X);box.Y=Math.Min(box.Y,b.Y);box.Right=Math.Max(box.Right,b.Right);box.Bottom=Math.Max(box.Bottom,b.Bottom);}}
            return box;
        }
    }
    [DataContract] public class Region {
        [DataMember] public string Id=Guid.NewGuid().ToString();
        [DataMember] public int Revision;
        [DataMember] public List<InkTrace> Traces=new List<InkTrace>();
        [DataMember] public string Text="";
        [DataMember] public string[] Alternatives=new string[0];
        [DataMember] public bool Manual;
        [DataMember] public bool Suppressed;
        [DataMember] public int RecognizedCount;
        [DataMember] public double AnchorX;
        [DataMember] public double AnchorY;
        [DataMember] public bool Closed;
        [DataMember] public bool InkOnly;
        [DataMember] public bool Converted;
        [DataMember] public Bounds ConvertedBounds;
        public Bounds Box { get { return Converted?ConvertedBounds:Bounds.Union(Traces.Select(t=>t.Box)); } }
    }
    public static class InkConversion {
        public static void Commit(Region region) {
            if(region.Converted)return;
            region.ConvertedBounds=region.Box;
            region.Converted=true;region.Closed=true;region.InkOnly=false;
            region.Traces.Clear();region.RecognizedCount=0;region.Revision++;
        }
    }
    [DataContract] public class InkDocument {
        [DataMember] public int Version=1;
        [DataMember] public string Id=Guid.NewGuid().ToString();
        [DataMember] public string Page="page-1";
        [DataMember] public string Layer="ink-1";
        [DataMember] public string Font="Segoe UI";
        [DataMember] public double FontSize=26;
        [DataMember] public string Layout="lines";
        [DataMember] public List<Region> Regions=new List<Region>();
    }
    public static class Storage {
        static readonly object saveGate=new object();
        public static byte[] Encode(InkDocument d) {using(var m=new MemoryStream()){new DataContractJsonSerializer(typeof(InkDocument)).WriteObject(m,d);return m.ToArray();}}
        public static InkDocument Decode(byte[] bytes) {
            using(var m=new MemoryStream(bytes)) {
                var d=(InkDocument)new DataContractJsonSerializer(typeof(InkDocument)).ReadObject(m);
                if(String.IsNullOrEmpty(d.Layout))d.Layout="lines";
                if(d.Version!=1 || d.Regions==null || d.Regions.Any(r=>r.Traces==null || r.Traces.Any(t=>t.Points==null || t.Points.Count==0 || t.Points.Any(p=>double.IsNaN(p.X)||double.IsInfinity(p.X)||double.IsNaN(p.Y)||double.IsInfinity(p.Y)))) || d.FontSize<8 || d.FontSize>100)
                    throw new InvalidDataException("Documento inválido ou versão incompatível.");
                foreach(var region in d.Regions.Where(r=>!r.InkOnly&&!String.IsNullOrEmpty(r.Text)&&r.Traces.Count>0&&r.RecognizedCount==r.Traces.Count))InkConversion.Commit(region);
                return d;
            }
        }
        public static void Save(string path, InkDocument d) {
            lock(saveGate){
            var temp=path+".tmp";
            using(var f=new FileStream(temp,FileMode.Create,FileAccess.Write,FileShare.None)){
                new DataContractJsonSerializer(typeof(InkDocument)).WriteObject(f,d);
                f.Flush(true);
            }
            if(File.Exists(path)) File.Replace(temp,path,path+".bak"); else File.Move(temp,path);
            }
        }
    }
    public static class DocumentSnapshots {
        // Completed traces are append-only values in the editor: after pen-up
        // neither their points nor device metadata are mutated. Snapshots own
        // their region/list containers and share those completed trace values.
        // Active ink is never inserted into a document until pen-up.
        public static InkDocument Copy(InkDocument d) {
            return new InkDocument {Version=d.Version,Id=d.Id,Page=d.Page,Layer=d.Layer,Font=d.Font,FontSize=d.FontSize,Layout=d.Layout,
                Regions=d.Regions.Select(r=>new Region {Id=r.Id,Revision=r.Revision,Text=r.Text,
                    Alternatives=(string[])r.Alternatives.Clone(),Manual=r.Manual,Suppressed=r.Suppressed,
                    RecognizedCount=r.RecognizedCount,AnchorX=r.AnchorX,AnchorY=r.AnchorY,Closed=r.Closed,InkOnly=r.InkOnly,Converted=r.Converted,ConvertedBounds=r.ConvertedBounds,
                    Traces=new List<InkTrace>(r.Traces)}).ToList()};
        }
    }
    public sealed class History {
        readonly Stack<InkDocument> undo=new Stack<InkDocument>(),redo=new Stack<InkDocument>();
        public void Record(InkDocument d){undo.Push(DocumentSnapshots.Copy(d));redo.Clear();}
        public void ConversionBarrier(){undo.Clear();redo.Clear();}
        public InkDocument Undo(InkDocument d){if(undo.Count==0)return d;redo.Push(DocumentSnapshots.Copy(d));return undo.Pop();}
        public InkDocument Redo(InkDocument d){if(redo.Count==0)return d;undo.Push(DocumentSnapshots.Copy(d));return redo.Pop();}
    }
    public static class Spatial {
        public const long RevisionWindowMs=8000;
        public static bool MayRevise(Region region,InkTrace next) {
            if(region.Converted||region.Manual||region.InkOnly)return false;
            if(String.IsNullOrEmpty(region.Text)||region.RecognizedCount<region.Traces.Count)return true;
            if(region.Traces.Count==0||next.Points.Count==0)return false;
            long elapsed=next.Points[0].Time-region.Traces.Last().Points.Last().Time;
            return elapsed<=RevisionWindowMs;
        }
        public static bool Intersects(Bounds a,Bounds b) {
            return a.X<=b.Right&&a.Right>=b.X&&a.Y<=b.Bottom&&a.Bottom>=b.Y;
        }
        public static Region AppendInk(InkDocument document,InkTrace trace) {
            var region=new Region {InkOnly=true,AnchorX=trace.Box.X,AnchorY=trace.Box.Y,Revision=1,Closed=true};
            region.Traces.Add(trace);document.Regions.Add(region);return region;
        }
        // A region is a local word-sized writing group, not an entire line.
        public static double WritingHeight(Region region) {
            // Short crossbars must not shrink the estimated height of a cursive
            // word and strand a later tilde/cedilla outside its revision window.
            double height=region.Box.Height;
            return height<8?20:Math.Max(12,Math.Min(80,height));
        }
        public static bool IsDot(InkTrace trace) {
            var box=trace.Box;
            return box.Right-box.X<=10 && box.Height<=10;
        }
        static bool IsVerticalSymbolPart(Region region,InkTrace trace) {
            // A dot above/below a compact body belongs to the same tentative
            // symbol (?!:; or dotted letter), regardless of the body's current
            // recognition. Words are not pulled into punctuation clusters.
            if(region.Traces.Count!=1)return false;
            var previous=region.Traces[0];
            if(!IsDot(previous)&&!IsDot(trace))return false;
            var dot=IsDot(trace)?trace.Box:previous.Box;
            var body=IsDot(trace)?previous.Box:trace.Box;
            double height=Math.Max(20,body.Height);
            if(body.Height>80 || body.Right-body.X>Math.Max(12,body.Height*1.1))return false;
            double gap=Math.Max(dot.Y-body.Bottom,body.Y-dot.Bottom);
            double dotCenter=(dot.X+dot.Right)/2;
            return gap>=0 && gap<=Math.Max(12,height*.8) &&
                dotCenter>=body.X-Math.Max(4,height*.25) &&
                dotCenter<=body.Right+Math.Max(4,height*.25);
        }
        static bool IsLateMark(Region region, Bounds mark) {
            var box=region.Box;
            double height=WritingHeight(region);
            // Only small overlapping additions can reopen a closed word. A new
            // word above/below or beside it must never reinterpret its content.
            if(box.Height>height*1.8 || mark.Right<box.X || mark.X>box.Right)return false;
            bool accent=mark.Height<=height*.55 && mark.Right-mark.X<=height*.9 &&
                mark.Bottom>=box.Y-height*.55 && mark.Y<=box.Y+height*.4;
            bool completion=mark.Height<=height*1.8 && mark.Right-mark.X<=height*.75 &&
                (mark.Y+mark.Bottom)/2>=box.Y-height*.2 &&
                (mark.Y+mark.Bottom)/2<=box.Bottom+height*.2;
            bool cedilla=mark.Height<=height*.45 && mark.Right-mark.X<=height*.7 &&
                mark.Y>=box.Bottom-height*.15 && mark.Y<=box.Bottom+height*.35;
            return accent||completion||cedilla;
        }
        public static Region Find(InkDocument d, InkTrace trace) {
            var b=trace.Box;
            var symbol=d.Regions.Where(r=>MayRevise(r,trace)&&IsVerticalSymbolPart(r,trace))
                .OrderBy(r=>Math.Abs((b.Y+b.Bottom)-(r.Box.Y+r.Box.Bottom))+Math.Abs((b.X+b.Right)-(r.Box.X+r.Box.Right)))
                .FirstOrDefault();
            if(symbol!=null)return symbol;
            // Prefer an actual accent overlap over proximity to another word.
            var marked=d.Regions.Where(r=>MayRevise(r,trace)&&IsLateMark(r,b))
                .OrderBy(r=>Math.Abs((b.X+b.Right)-(r.Box.X+r.Box.Right))).FirstOrDefault();
            if(marked!=null)return marked;
            var active=d.Regions.LastOrDefault(r=>MayRevise(r,trace)&&!r.Closed);
            if(active==null)return null;
            var a=active.Box;
            double h=WritingHeight(active);
            double overlap=Math.Min(a.Bottom,b.Bottom)-Math.Max(a.Y,b.Y);
            bool sameLine=overlap>=Math.Min(h,Math.Max(1,b.Height))*.25;
            double horizontalGap=Math.Max(b.X-a.Right,a.X-b.Right);
            bool local=horizontalGap<=Math.Max(9,h*.4);
            return sameLine&&local?active:null;
        }
        public static Region Append(InkDocument document, InkTrace trace) {
            var region=Find(document,trace);
            if(region==null) {
                if(document.Regions.Any(r=>!r.Converted&&!r.InkOnly&&!String.IsNullOrEmpty(r.Text)&&!MayRevise(r,trace)&&Intersects(r.Box,trace.Box)))
                    return AppendInk(document,trace);
                foreach(var previous in document.Regions)previous.Closed=true;
                region=new Region {AnchorX=trace.Box.X,AnchorY=trace.Box.Y};
                document.Regions.Add(region);
            }
            region.Traces.Add(trace);
            region.Revision++;
            region.Suppressed=false;
            return region;
        }
        public static List<List<InkTrace>> Groups(IList<InkTrace> traces, bool words) {
            var groups=new List<List<InkTrace>>();
            var sorted=traces.OrderBy(t=>t.Box.X).ToArray();
            var heights=sorted.Select(t=>t.Box.Height).Where(height=>height>8).OrderBy(height=>height).ToArray();
            double h=heights.Length==0?30:heights[heights.Length/2];
            var gaps=new List<double>(); double right=double.NegativeInfinity;
            foreach(var t in sorted) {
                double gap=t.Box.X-right;
                var recent=gaps.OrderBy(x=>x).ToArray();
                double typical=recent.Length==0?h*.12:recent[recent.Length/2];
                double threshold=words?Math.Max(h*.6,typical*2.5):h*.08;
                if(groups.Count==0||gap>threshold)groups.Add(new List<InkTrace>());
                groups[groups.Count-1].Add(t);
                if(gap>0 && gap<h*.6)gaps.Add(gap);
                right=Math.Max(right,t.Box.Right);
            }
            // Restore temporal order within each spatial component, including late accents.
            return groups.Select(g=>g.OrderBy(t=>t.Points[0].Time).ToList()).ToList();
        }
    }
    public sealed class Ticket {
        public string Session,Document,Page,Layer,Region; public int Revision;
        public bool Matches(string session,InkDocument d,Region r) {
            return Session==session && Document==d.Id && Page==d.Page && Layer==d.Layer && r!=null && Region==r.Id && Revision==r.Revision && !r.Manual && !r.Suppressed && !r.InkOnly && !r.Converted;
        }
    }
}

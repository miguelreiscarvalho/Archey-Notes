using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Globalization;
using System.Collections.Generic;
using Tracos;

// Adapter for the user's Tracos core. No WPF surface, clipboard, recovery file or editor history.
public static class RecognitionHost {
    static readonly CultureInfo Invariant = CultureInfo.InvariantCulture;
    static string Enc(string value) { return Convert.ToBase64String(Encoding.UTF8.GetBytes(value)); }
    static double Number(string value) {
        double result = Double.Parse(value, Invariant);
        if (Double.IsNaN(result) || Double.IsInfinity(result) || Math.Abs(result)>1000000) throw new InvalidDataException("Coordenada invalida.");
        return result;
    }
    [STAThread] public static int Main(string[] args) {
        Console.InputEncoding = new UTF8Encoding(false);
        Console.OutputEncoding = new UTF8Encoding(false);
        try {
            if (args.Contains("--probe")) {
                var availableEngine = new Microsoft.Ink.Recognizers().Cast<Microsoft.Ink.Recognizer>().FirstOrDefault(r=>r.Languages.Contains((short)1046));
                if(availableEngine==null)throw new InvalidOperationException("Reconhecedor pt-BR ausente.");
                Console.WriteLine("ENGINE\t"+Enc(availableEngine.Name)); return 0;
            }
            var header=(Console.ReadLine()??"").Split('\t');
            if(header.Length!=2 || header[0]!="TRACOS1") throw new InvalidDataException("Protocolo invalido.");
            int count=Int32.Parse(header[1],Invariant);
            if(count<1 || count>256) throw new InvalidDataException("Quantidade de tracos invalida.");
            var document=new InkDocument(); int total=0;
            for(int i=0;i<count;i++) {
                var line=(Console.ReadLine()??"").Split('\t');
                if(line.Length!=2 || line[0]!=i.ToString(Invariant)) throw new InvalidDataException("Identificador invalido.");
                int length=Int32.Parse(line[1],Invariant); total+=length;
                if(length<1 || total>200000)throw new InvalidDataException("Limite de pontos excedido.");
                var trace=new InkTrace{Id=i.ToString(Invariant),Device="Xournal++ native"};
                for(int n=0;n<length;n++) {
                    var p=(Console.ReadLine()??"").Split('\t');
                    if(p.Length!=4)throw new InvalidDataException("Ponto incompleto.");
                    trace.Points.Add(new InkPoint{X=Number(p[0])*96/72,Y=Number(p[1])*96/72,Time=Int64.Parse(p[2],Invariant),Pressure=(float)Number(p[3])});
                }
                // Preserve the original temporal order so dots and accents can join the current word.
                Spatial.Append(document,trace);
            }
            var engine=new WindowsInkEngine();
            Console.WriteLine("TRACOS1");
            foreach(var region in document.Regions) {
                var answer=engine.Recognize(region.Traces,1046);
                if(String.IsNullOrWhiteSpace(answer.Text))continue;
                var box=region.Box;
                Console.WriteLine("TEXT\t"+Enc(answer.Text)+"\t"+(box.X*72/96).ToString("R",Invariant)+"\t"+(box.Y*72/96).ToString("R",Invariant)+"\t"+String.Join(",",region.Traces.Select(t=>t.Id)));
            }
            return 0;
        } catch(Exception error) {
            Console.WriteLine("ERROR\t"+Enc(error.Message)); return 2;
        }
    }
}

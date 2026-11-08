using System;
using System.Linq;
using System.Collections.Generic;
using System.Drawing;
using System.Text;
using Microsoft.Ink;

namespace Tracos {
    public sealed class WindowsInkEngine : IRecognitionEngine {
        readonly bool resolvePunctuation;
        public WindowsInkEngine(bool resolvePunctuation=true){this.resolvePunctuation=resolvePunctuation;}
        public InkAnswer Recognize(IList<InkTrace> traces, int language) {
            {
                var recognizers = new Recognizers();
                Recognizer chosen = null;
                foreach (Recognizer r in recognizers)
                    if (r.Languages.Contains((short)language)) { chosen = r; break; }
                if (chosen == null) throw new InvalidOperationException("Reconhecedor pt-BR ausente. Instale Manuscrito nas opções de idioma do Windows.");
                using (var context = chosen.CreateRecognizerContext())
                using (var ink = new Ink()) {
                    foreach (var trace in traces) {
                        // WPF device-independent pixels -> HIMETRIC (0.01 mm).
                        var points = trace.Points.Select(p => new Point((int)Math.Round(p.X*2540/96), (int)Math.Round(p.Y*2540/96))).ToArray();
                        if (points.Length == 1) points = new[] {points[0], points[0]};
                        if (points.Length > 0) ink.CreateStroke(points);
                    }
                    context.Strokes = ink.Strokes;
                    RecognitionStatus status;
                    var result = context.Recognize(out status);
                    if (status != RecognitionStatus.NoError || result == null) return new InkAnswer {Engine=chosen.Name};
                    var alternatives=result.GetAlternatesFromSelection().Cast<RecognitionAlternate>().Take(10).Select(a=>a.ToString().Normalize(NormalizationForm.FormC)).ToArray();
                    var top=result.TopString.Normalize(NormalizationForm.FormC);
                    return new InkAnswer {Text=resolvePunctuation?PunctuationCandidates.Select(top,alternatives,traces):top, Engine=chosen.Name,
                        Alternatives=alternatives};
                }
            }
        }
    }
}

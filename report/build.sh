latexmk -e '$pdflatex=q/pdflatex %O -shell-escape -interaction=nonstopmode %S/' -pdf \
&& texcount main.tex | grep "Words in text:" \
&& open main.pdf
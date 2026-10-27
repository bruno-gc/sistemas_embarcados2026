from flask import Flask, render_template

app = Flask(__name__)

@app.route("/")
def index():
    return "Helooowww world eng. Computacional!!!!!!!"
  
@app.route("/console")
def console():
    return render_template("console.html")
  
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
